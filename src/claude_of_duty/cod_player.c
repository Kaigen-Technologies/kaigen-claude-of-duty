// Claude of Duty player: the ECS systems, health and the public schedule over the movement, ledge and camera files.

// ---- health ----

hz_internal f32 cod_health_fraction(const CodHealth *hp) { return m_saturatef(hp->value / COD_PLAYER_MAX_HEALTH); }

hz_internal void cod_health_reset(CodHealth *hp) {
  *hp = (CodHealth){.value = COD_PLAYER_MAX_HEALTH, .since_damage = 999.0f};
}

hz_internal void cod_health_add_suppression(CodHealth *hp, f32 amount) { hp->suppression = m_saturatef(hp->suppression + amount); }

// reuses the slot pointing the most similar way, else the oldest
hz_internal void cod_health_push_indicator(CodHealth *hp, f32 angle, f32 amount, v3 from) {
  CodDamageIndicator *slot = NULL;
  CodDamageIndicator *oldest = NULL;
  for (u32 k = 0; k < COD_PLAYER_MAX_INDICATORS; k++) {
    CodDamageIndicator *i = &hp->indicators[k];
    if (!i->active || m_fabsf(angle - i->angle) < 0.5f) {
      slot = i;
      break;
    }
    if (!oldest || i->life > oldest->life)
      oldest = i;
  }
  if (!slot)
    slot = oldest;
  f32 kept = slot->active ? slot->amount * 0.5f : 0.0f;
  *slot = (CodDamageIndicator){.active = true, .angle = angle, .amount = m_maxf(kept, amount), .life = 0.0f, .from = from};
}

// damage with its felt response: the indicator, the flash, suppression, a camera punch away from the hit and trauma
hz_internal f32 cod_health_damage(CodHealth *hp, CodView *v, f32 yaw, v3 eye, f32 amount, v3 from, b32 directionless) {
  if (hp->dead || amount <= 0.0f)
    return 0.0f;
  f32 before = hp->value;
  hp->value = m_maxf(0.0f, hp->value - amount);
  hp->since_damage = 0.0f;
  hp->regenerating = false;
  f32 dealt = before - hp->value;

  // the direction in view space: forward at yaw is (-sin, -cos), right is (cos, -sin)
  f32 angle = 0.0f;
  if (!directionless) {
    f32 dx = from.x - eye.x, dz = from.z - eye.z;
    f32 f = -m_sinf(yaw) * dx - m_cosf(yaw) * dz;
    f32 r = m_cosf(yaw) * dx - m_sinf(yaw) * dz;
    angle = m_atan2f(r, f);
    cod_health_push_indicator(hp, angle, dealt, from);
  }

  f32 severity = m_saturatef(dealt / 45.0f);
  hp->hit_flash = m_saturatef(hp->hit_flash + COD_HIT_FLASH * (0.4f + severity));
  cod_health_add_suppression(hp, COD_SUPPRESSION_PER_HIT * (0.5f + severity));
  f32 s = 0.6f + severity * 1.9f;
  cod_view_add_recoil(v, (1.1f + severity) * COD_DEG * s * 0.7f, -m_sinf(angle) * (1.4f * COD_DEG) * s,
                      -m_sinf(angle) * (2.2f * COD_DEG) * s, 0.008f * s);
  cod_view_add_trauma(v, 0.22f * s);

  if (hp->hit_count < COD_PLAYER_MAX_HITS)
    hp->hits[hp->hit_count++] = (CodPlayerHit){dealt, directionless ? eye : from, directionless};
  if (hp->value <= 0.0f) {
    hp->dead = true;
    hp->died = true;
  }
  return dealt;
}

// regeneration after a delay, the suppression and flash pools, the low-health weight and the heartbeat
hz_internal void cod_health_update(CodHealth *hp, CodView *v, f32 dt) {
  if (dt <= 0.0f)
    return;
  hp->since_damage += dt;
  if (!hp->dead && hp->value < COD_PLAYER_MAX_HEALTH && hp->since_damage > COD_HEALTH_REGEN_DELAY) {
    hp->regenerating = true;
    // ramped in so the recovery has a shape rather than a step
    f32 ramp = m_saturatef((hp->since_damage - COD_HEALTH_REGEN_DELAY) / COD_HEALTH_REGEN_RAMP);
    hp->value = m_minf(COD_PLAYER_MAX_HEALTH, hp->value + COD_HEALTH_REGEN_RATE * ramp * dt);
  } else if (hp->value >= COD_PLAYER_MAX_HEALTH) {
    hp->regenerating = false;
  }

  hp->suppression = m_maxf(0.0f, hp->suppression - COD_SUPPRESSION_DECAY * dt);
  hp->hit_flash = cod_approach(hp->hit_flash, 0.0f, COD_HIT_FLASH_TAU, dt);
  for (u32 k = 0; k < COD_PLAYER_MAX_INDICATORS; k++) {
    CodDamageIndicator *i = &hp->indicators[k];
    if (!i->active)
      continue;
    i->life += dt;
    if (i->life > COD_HEALTH_INDICATOR_TIME)
      i->active = false;
  }

  f32 f = cod_health_fraction(hp);
  f32 target = m_saturatef((COD_HEALTH_LOW - f) / COD_HEALTH_LOW);
  hp->effect = cod_approach(hp->effect, target, 0.25f, dt);

  if (hp->effect > 0.02f) {
    f32 freq = m_lerpf(COD_HEARTBEAT_MIN, COD_HEARTBEAT_MAX, m_saturatef(1.0f - f / COD_HEALTH_LOW));
    hp->beat_phase += dt * freq;
    if (hp->beat_phase >= 1.0f) {
      hp->beat_phase -= m_floorf(hp->beat_phase);
      hp->beat = true;
      hp->beat_strength = hp->effect;
    }
    // lub-dub: two gaussian thumps 0.16 of a cycle apart
    f32 t = hp->beat_phase;
    f32 a = (t - 0.06f) / 0.035f, b = (t - 0.22f) / 0.045f;
    hp->pulse = (m_expf(-0.5f * a * a) + 0.62f * m_expf(-0.5f * b * b)) * hp->effect;
  } else {
    hp->beat_phase = 0.0f;
    hp->pulse = 0.0f;
  }

  // suppression makes the camera shake a little
  if (hp->suppression > 0.02f)
    cod_view_add_trauma(v, hp->suppression * COD_SUPPRESSION_SHAKE_SCALE * dt);
}

// ---- placement ----

// physics owns the exact floor: false until the world's colliders are in the static layer, so the capsule never starts embedded
hz_internal b32 cod_player_spawn(EcsWorld *w, const CodWorld *world, CodSpawnPoint *out) {
  CodSpawnPoint sp = world->spawn_count > 0 ? world->spawns[0] : (CodSpawnPoint){0};
  f32 floor_y = character_spawn_height(w, sp.position.x, sp.position.z, sp.position.y + 6.0f, 16.0f, HZ_INF);
  b32 found = floor_y < HZ_INF;
  if (found)
    sp.position.y = floor_y;
  *out = sp;
  return found;
}

// the teleport and respawn: the machine restarts where the character is placed, the camera springs clear
hz_internal void cod_player_place(CodMove *m, CodView *v, CodHealth *hp, CharacterInput *ci, CharacterTuning *tun, v3 feet, f32 yaw,
                                  f32 pitch, b32 full_health) {
  cod_move_reset(m, tun);
  m->yaw = yaw;
  m->pitch = m_clampf(pitch, -COD_PLAYER_PITCH_LIMIT, COD_PLAYER_PITCH_LIMIT);
  m->yaw_rate = 0.0f;
  // the character's yaw 0 faces +Z, the view's faces -Z
  // `feet` is the floor; the character rests its skin above it, or every headroom test starts embedded
  v3 at = {feet.x, feet.y + COD_CHAR_SKIN, feet.z};
  cod_move_teleport(m, ci, at, m_wrap_pi(yaw + COD_PI));
  m->feet = m->prev_feet = at;
  m->grounded = false;
  m->was_grounded = false;
  m->suppress_landing = true;
  m->ev.reset = true;
  m->placed = true;
  cod_view_reset(v);
  if (full_health) {
    cod_health_reset(hp);
    hp->respawned = true;
  }
}

// ---- input: placement, the look and the frame's intent ----

ecs_system(cod_player_input_system, EcsInOut(CodMove), EcsInOut(CodView), EcsInOut(CodHealth), EcsInOut(CharacterInput),
           EcsInOut(CharacterTuning), EcsIn(Character), EcsResIn(CodWorld), EcsResIn(EcsPhysicsWorld)) {
  // the system takes 8 terms at most: the input, dev, clock, settings and aim resources are settled by an earlier flush
  const CodInput *in = ecs_resource_get(it->world, CodInput);
  const CodDev *d = ecs_resource_get(it->world, CodDev);
  const CodClock *clock = ecs_resource_get(it->world, CodClock);
  const CodWorld *world = ecs_sys_get_resource(CodWorld);
  const CodSettings *settings = ecs_resource_get(it->world, CodSettings);
  const CodPlayerAim *aim = ecs_resource_get(it->world, CodPlayerAim);
  for (i32 i = 0; i < it->count; i++) {
    CodMove *m = ecs_sys_get_component_mut(CodMove, i);
    CodView *v = ecs_sys_get_component_mut(CodView, i);
    CodHealth *hp = ecs_sys_get_component_mut(CodHealth, i);
    CharacterInput *ci = ecs_sys_get_component_mut(CharacterInput, i);
    CharacterTuning *tun = ecs_sys_get_component_mut(CharacterTuning, i);
    const Character *c = ecs_sys_get_component(Character, i);
    m->ev = (CodPlayerEvents){0};
    m->steps_pending = 0;
    hp->hit_count = 0;
    hp->died = false;
    hp->respawned = false;
    hp->beat = false;
    if (d->state == COD_STATE_LOADING || !world->ready)
      continue;

    b32 restart = m->applied_restart != d->restart_serial;
    if (!m->placed || restart || m->applied_pose != d->pose_serial) {
      CodSpawnPoint spawn;
      if (!cod_player_spawn(it->world, world, &spawn) && !d->pose)
        continue;
      cod_player_place(m, v, hp, ci, tun, d->pose ? d->feet : spawn.position, d->pose ? d->yaw : spawn.yaw, d->pose ? d->pitch : 0.0f,
                       restart || !m->placed);
      m->applied_restart = d->restart_serial;
      m->applied_pose = d->pose_serial;
    }
    if (c->position.y < COD_PLAYER_FALL_RESET_Y) {
      CodSpawnPoint spawn;
      cod_player_spawn(it->world, world, &spawn);
      cod_player_place(m, v, hp, ci, tun, spawn.position, spawn.yaw, 0.0f, false);
    }
    if (hp->dead && hp->dead_time > COD_PLAYER_RESPAWN_DELAY) {
      CodSpawnPoint spawn;
      cod_player_spawn(it->world, world, &spawn);
      cod_player_place(m, v, hp, ci, tun, spawn.position, spawn.yaw, 0.0f, true);
    }

    f32 dt = clock->live ? clock->dt : 0.0f;
    if (hp->dead)
      hp->dead_time += dt;
    else
      hp->dead_time = 0.0f;
    b32 controls = clock->controls && !hp->dead;
    m->controls = controls;
    m->move_speed_scale = aim->move_speed_scale > 0.0f ? aim->move_speed_scale : 1.0f;

    // the aim: the weapons' curve when they drive it, the button's own blend otherwise
    m->ads_requested = controls && in->ads && !m->mantle.active && !m->sliding;
    if (aim->driven)
      m->ads = m_saturatef(aim->ads);
    else
      m->ads = cod_approach(m->ads, m->ads_requested ? 1.0f : 0.0f, 0.075f, dt);

    // the look: consumed once per frame before the fixed steps, so the movement sees this frame's yaw
    m->yaw_rate = 0.0f;
    if (controls && dt > 0.0f) {
      f32 sens = COD_PLAYER_MOUSE_SENSITIVITY * settings->sensitivity * m_lerpf(1.0f, COD_PLAYER_ADS_SENS_SCALE, m_saturatef(m->ads));
      f32 dyaw = -in->look.x * sens;
      f32 dpitch = (settings->invert_y ? in->look.y : -in->look.y) * sens;
      // mantles are rooted: you keep your head, but the shoulders are committed
      if (m->mantle.active) {
        dyaw *= 0.55f;
        dpitch *= 0.55f;
      }
      m->yaw = m_wrap_pi(m->yaw + dyaw);
      m->pitch = m_clampf(m->pitch + dpitch, -COD_PLAYER_PITCH_LIMIT, COD_PLAYER_PITCH_LIMIT);
      m->yaw_rate = dyaw / dt;
    }

    CodCmd *cmd = &m->cmd;
    *cmd = (CodCmd){0};
    if (!controls)
      continue;
    cmd->move = in->move;
    cmd->jump_held = in->jump;
    cmd->sprint_held = in->sprint;
    cmd->lean_left = in->lean_left;
    cmd->lean_right = in->lean_right;
    m->edge_jump |= in->jump_pressed;
    m->edge_crouch |= in->crouch_pressed;
    m->edge_prone |= in->prone_pressed;
    m->edge_sprint |= in->sprint_pressed;
    if (in->jump_pressed)
      m->jump_buffer = COD_MOVE_JUMP_BUFFER;
    // tactical sprint is a double tap of sprint
    if (in->sprint_pressed) {
      if (clock->time - m->last_sprint_press < COD_MOVE_TAC_TAP_WINDOW && m->tac_lock <= 0.0f)
        m->tac_requested = true;
      m->last_sprint_press = clock->time;
    }
  }
}

// ---- the fixed step: decisions, the character block, bookkeeping ----

ecs_system(cod_move_pre_system, EcsInOut(CodMove), EcsInOut(CharacterInput), EcsInOut(CharacterTuning), EcsIn(Character),
           EcsResIn(EcsPhysicsWorld), EcsExternIn(CodSurfaceTag)) {
  CodPhys phys = cod_phys_make(it->world);
  for (i32 i = 0; i < it->count; i++) {
    CodMove *m = ecs_sys_get_component_mut(CodMove, i);
    if (!m->placed)
      continue;
    cod_move_step(m, it->world, &phys, ecs_sys_get_component(Character, i), ecs_sys_get_component_mut(CharacterInput, i),
                  ecs_sys_get_component_mut(CharacterTuning, i), it->delta_time);
  }
}

ecs_system(cod_move_post_system, EcsInOut(CodMove), EcsIn(Character), EcsResIn(EcsPhysicsWorld), EcsExternIn(CodSurfaceTag)) {
  CodPhys phys = cod_phys_make(it->world);
  for (i32 i = 0; i < it->count; i++) {
    CodMove *m = ecs_sys_get_component_mut(CodMove, i);
    if (!m->placed)
      continue;
    cod_move_post(m, it->world, &phys, ecs_sys_get_component(Character, i), it->delta_time);
  }
}

// ---- the frame: movement events into the camera and health, the camera feel ----

ecs_system(cod_player_frame_system, EcsInOut(CodMove), EcsInOut(CodView), EcsInOut(CodHealth), EcsIn(Character), EcsResIn(CodPlayerAim)) {
  // the flow's clock, dev and settings resources are settled by the flow's flush; the aim is the weapons' last frame
  const CodClock *clock = ecs_resource_get(it->world, CodClock);
  const CodDev *d = ecs_resource_get(it->world, CodDev);
  const CodSettings *settings = ecs_resource_get(it->world, CodSettings);
  const CodPlayerAim *aim = ecs_sys_get_resource(CodPlayerAim);
  f32 dt = clock->live ? clock->dt : 0.0f;
  for (i32 i = 0; i < it->count; i++) {
    CodMove *m = ecs_sys_get_component_mut(CodMove, i);
    if (!m->placed)
      continue;
    CodView *v = ecs_sys_get_component_mut(CodView, i);
    CodHealth *hp = ecs_sys_get_component_mut(CodHealth, i);
    const Character *c = ecs_sys_get_component(Character, i);
    CodPlayerEvents *ev = &m->ev;

    if (ev->landed) {
      f32 mag = cod_view_on_land(v, ev->land_speed);
      // CoD only hurts you past a real drop
      if (ev->land_speed > COD_LAND_DAMAGE_SPEED && !d->god)
        cod_health_damage(hp, v, m->yaw, v->eye_pos, (ev->land_speed - COD_LAND_DAMAGE_SPEED) * COD_LAND_DAMAGE_PER_SPEED, v->eye_pos, true);
      if (mag > 0.35f)
        m->foot_hold = COD_FOOT_LAND_HOLD;
    }
    for (u32 k = 0; k < m->steps_pending; k++)
      cod_view_on_footstep(v, m->step_running, m->step_stance);
    if (ev->jumped)
      cod_view_add_recoil(v, -0.35f * COD_DEG, 0.0f, 0.0f, 0.004f);
    if (ev->slide_started)
      cod_view_on_slide_start(v, m->slide_side);
    if (ev->mantled)
      cod_view_add_trauma(v, ev->mantle_kind == COD_LEDGE_VAULT ? 0.08f : 0.14f);

    cod_health_update(hp, v, dt);
    f32 base_fov = d->fov > 0.0f ? d->fov : settings->fov;
    f32 ads_fov = aim->ads_fov_scale > 0.0f ? aim->ads_fov_scale : COD_PLAYER_ADS_FOV_SCALE;
    cod_view_update(v, m, c, cod_health_fraction(hp), hp->suppression, hp->dead, base_fov, ads_fov, dt, clock->alpha);

    u32 sig = m->state | m->stance << 4 | (u32)m->grounded << 6 | (u32)m->mantle.active << 7 | (u32)m->sprinting << 8 |
              (u32)m->tactical << 9 | (u32)m->sliding << 10 | (u32)(m->ads > 0.5f) << 11;
    if (sig != m->state_sig) {
      m->state_sig = sig;
      ev->state_changed = true;
    }
  }
}

// ---- publishing ----

hz_internal void cod_player_state_write(CodPlayerState *out, const CodMove *m, const CodView *v, const CodHealth *hp, const Character *c,
                                        EcsEntity e) {
  out->entity = e;
  out->ready = m->placed;
  out->controls = m->controls;
  out->feet = v->feet;
  out->eye = v->eye_pos;
  out->velocity = c->velocity;
  out->forward = quat_rotate(v->rot, V3_BACK); // the camera looks down its -Z
  out->view_rot = v->rot;
  out->yaw = m->yaw;
  out->pitch = m->pitch;
  out->fov = v->fov;
  out->height = cod_stance_def(m->stance).height;
  out->eye_height = v->eye;
  out->state = m->state;
  out->stance = m->stance;
  out->grounded = m->grounded;
  out->sprinting = m->sprinting;
  out->tactical_sprint = m->tactical;
  out->sliding = m->sliding;
  out->mantling = m->mantle.active;
  out->lean = m->lean_amount;
  out->slide_progress = cod_slide_progress(m);
  out->speed = m->hspeed;
  out->move_amount = v->move_amount;
  out->bob_phase = v->bob_phase;
  out->ads_requested = m->ads_requested;
  out->ads = m->ads;
  out->health = hp->value;
  out->health_fraction = cod_health_fraction(hp);
  out->low_health = out->health_fraction < COD_HEALTH_LOW;
  out->critical_health = out->health_fraction < COD_HEALTH_CRITICAL;
  out->regenerating = hp->regenerating;
  out->dead = hp->dead;
  out->suppression = hp->suppression;
  out->trauma = v->shake_amount;
  out->low_effect = hp->effect;
  out->heartbeat_pulse = hp->pulse;
  out->hit_flash = hp->hit_flash;
  for (u32 k = 0; k < COD_PLAYER_MAX_INDICATORS; k++)
    out->indicators[k] = hp->indicators[k];
  out->view_kick = v->view_kick;
  out->view_punch = v->view_punch;
  out->events = m->ev;
  out->events.hit_count = hp->hit_count;
  for (u32 k = 0; k < hp->hit_count; k++)
    out->events.hits[k] = hp->hits[k];
  out->events.died = hp->died;
  out->events.respawned = hp->respawned;
  out->events.heartbeat = hp->beat;
  out->events.heartbeat_strength = hp->beat_strength;
}

// the player's view goes onto the camera entity; CameraRigState and CodPlayerState are published from the same row
ecs_system(cod_camera_publish_system, EcsIn(CodMove), EcsIn(CodView), EcsIn(CodHealth), EcsIn(Character), EcsExternInOut(Camera),
           EcsResIn(CodCourse), EcsResInOut(CameraRigState), EcsResInOut(CodPlayerState)) {
  CameraRigState *rig = ecs_sys_get_resource_mut(CameraRigState);
  CodPlayerState *out = ecs_sys_get_resource_mut(CodPlayerState);
  const CodCourse *course = ecs_sys_get_resource(CodCourse);
  for (i32 i = 0; i < it->count; i++) {
    const CodMove *m = ecs_sys_get_component(CodMove, i);
    const CodView *v = ecs_sys_get_component(CodView, i);
    const CodHealth *hp = ecs_sys_get_component(CodHealth, i);
    const Character *c = ecs_sys_get_component(Character, i);
    EcsEntity e = it->entities[i];
    if (!m->placed)
      continue;
    Camera *cam = ecs_component_get_mut(it->world, v->camera, Camera);
    if (cam) {
      cam->pos = v->eye_pos;
      cam->rot = v->rot;
      cam->fov = v->fov;
      ecs_component_was_modified(it->world, v->camera, Camera);
    }

    rig->position = v->eye_pos;
    rig->rotation = v->rot;
    rig->focus = v->feet;
    rig->yaw = m->yaw;
    rig->pitch = m->pitch;
    rig->fov = v->fov;
    rig->distance = 0.0f;
    rig->zoom = m->ads;
    rig->bob_phase = v->bob_phase;
    rig->move_amount = v->move_amount;
    rig->shake = v->shake_amount;
    rig->ready = true;
    cod_player_state_write(out, m, v, hp, c, e);
    if (course->trace) {
      log_info("player t=% feet=% % % vel=% % % hs=% st=% sn=% gr=% eye=% % % fov=% yaw=% pitch=% lean=% hp=% ads=% ls=% fs=% mk=% ind=% eff=% flash=% sup=% tr=% pulse=% kp=% kr=% dead=%",
               fmt_f32_p(ecs_resource_get(it->world, CodClock)->time, 3), fmt_f32_p(c->position.x, 3), fmt_f32_p(c->position.y, 3),
               fmt_f32_p(c->position.z, 3), fmt_f32_p(c->velocity.x, 3), fmt_f32_p(c->velocity.y, 3), fmt_f32_p(c->velocity.z, 3),
               fmt_f32_p(m->hspeed, 3), fmt_u32(m->state), fmt_u32(m->stance), fmt_b32(m->grounded), fmt_f32_p(v->eye_pos.x, 3),
               fmt_f32_p(v->eye_pos.y, 3), fmt_f32_p(v->eye_pos.z, 3), fmt_f32_p(v->fov, 2), fmt_f32_p(m->yaw, 3), fmt_f32_p(m->pitch, 3), fmt_f32_p(m->lean_amount, 3),
               fmt_f32_p(hp->value, 1), fmt_f32_p(m->ads, 2), fmt_f32_p(m->ev.landed ? m->ev.land_speed : 0.0f, 2),
               fmt_u32(m->ev.footstep ? m->ev.footstep_surface + 1 : 0), fmt_u32(m->ev.mantled ? m->ev.mantle_kind : 0),
               fmt_f32_p(hp->indicators[0].active ? hp->indicators[0].angle : 9.0f, 3), fmt_f32_p(hp->effect, 3),
               fmt_f32_p(hp->hit_flash, 3), fmt_f32_p(hp->suppression, 3), fmt_f32_p(v->shake_amount, 3), fmt_f32_p(hp->pulse, 3),
               fmt_f32_p(m_degrees(v->view_kick.x), 3), fmt_f32_p(m_degrees(v->view_kick.z), 3), fmt_b32(hp->dead));
    }
  }
}

// ---- the intake: damage, heals, trauma, recoil and explosions the writers queued this frame ----

// the CodWeaponState term orders it after the weapon update, which adds the shots' recoil to the intake
ecs_system(cod_player_intake_system, EcsInOut(CodPlayerIntake), EcsInOut(CodHealth), EcsInOut(CodView), EcsIn(CodMove),
           EcsResIn(CodExplosions), EcsResIn(CodWeaponState), EcsResIn(EcsPhysicsWorld), EcsExternIn(CodSurfaceTag)) {
  const CodDev *d = ecs_resource_get(it->world, CodDev);
  const CodExplosions *ex = ecs_sys_get_resource(CodExplosions);
  CodPhys phys = cod_phys_make(it->world);
  for (i32 i = 0; i < it->count; i++) {
    const CodPlayerIntake *q = ecs_sys_get_component(CodPlayerIntake, i);
    b32 queued = q->hits.len > 0 || q->heal != 0.0f || q->trauma != 0.0f || q->suppression != 0.0f || q->recoil_pitch != 0.0f ||
                 q->recoil_yaw != 0.0f || q->recoil_roll != 0.0f || q->recoil_punch != 0.0f || q->kick_pitch != 0.0f ||
                 q->kick_yaw != 0.0f || q->kick_roll != 0.0f || q->debug_damage != 0.0f || q->debug_suppression != 0.0f ||
                 q->debug_trauma != 0.0f;
    if (!queued && ex->count == 0)
      continue;
    CodHealth *hp = ecs_sys_get_component_mut(CodHealth, i);
    CodView *v = ecs_sys_get_component_mut(CodView, i);
    const CodMove *m = ecs_sys_get_component(CodMove, i);
    if (!m->placed)
      continue;
    f32 yaw = m->yaw;
    v3 eye = v->eye_pos;

    if (queued) {
      CodPlayerIntake *in = ecs_sys_get_component_mut(CodPlayerIntake, i);
      if (in->debug_damage != 0.0f)
        cod_player_intake_damage(in, in->debug_damage, v3_add(eye, v3_scale(quat_rotate(v->rot, V3_BACK), 10.0f)));
      for (u32 h = 0; h < in->hits.len && !d->god; h++)
        cod_health_damage(hp, v, yaw, eye, in->hits.items[h].amount, in->hits.items[h].from, in->hits.items[h].directionless);
      if (in->heal > 0.0f && !hp->dead)
        hp->value = m_minf(COD_PLAYER_MAX_HEALTH, hp->value + in->heal);
      cod_view_add_trauma(v, in->trauma + in->debug_trauma);
      cod_health_add_suppression(hp, in->suppression + in->debug_suppression);
      cod_view_add_recoil(v, in->recoil_pitch, in->recoil_yaw, in->recoil_roll, in->recoil_punch);
      cod_view_add_kick(v, in->kick_pitch, in->kick_yaw, in->kick_roll);
      *in = (CodPlayerIntake){0};
    }

    // a blast shakes and suppresses at range; walls stop the wound, not the shake
    for (u32 k = 0; k < ex->count; k++) {
      const CodExplosionEvent *e = &ex->items[k];
      f32 radius = e->radius > 0.0f ? e->radius : 5.0f;
      f32 dist = v3_distance(e->position, eye);
      if (dist > radius * 1.6f)
        continue;
      CodHit wall;
      b32 clear = !cod_phys_ray(&phys, e->position, eye, &wall);
      f32 falloff = m_powf(m_saturatef(1.0f - dist / radius), 1.6f);
      cod_view_add_trauma(v, m_saturatef(falloff * 1.4f));
      cod_health_add_suppression(hp, COD_SUPPRESSION_PER_EXPLOSION * falloff);
      if (clear && falloff > 0.02f && !d->god)
        cod_health_damage(hp, v, yaw, eye, (e->damage > 0.0f ? e->damage : 90.0f) * falloff, e->position, false);
    }
  }
}

// the same after the intake: health, hits, death and the trauma it added
ecs_system(cod_player_consolidate, EcsIn(CodMove), EcsIn(CodView), EcsIn(CodHealth), EcsIn(Character), EcsResInOut(CodPlayerState)) {
  CodPlayerState *out = ecs_sys_get_resource_mut(CodPlayerState);
  for (i32 i = 0; i < it->count; i++) {
    const CodMove *m = ecs_sys_get_component(CodMove, i);
    if (m->placed)
      cod_player_state_write(out, m, ecs_sys_get_component(CodView, i), ecs_sys_get_component(CodHealth, i),
                             ecs_sys_get_component(Character, i), it->entities[i]);
  }
}

// the fixed loop's remainder into the alpha the character pose blends by
ecs_system(cod_player_alpha_system, EcsResIn(CodClock), EcsResInOut(CharacterFrame)) {
  ecs_sys_get_resource_mut(CharacterFrame)->alpha = ecs_sys_get_resource(CodClock)->alpha;
}

// ---- the dev test course ----

hz_internal void cod_course_box(EcsWorld *w, CodCourse *c, const char *name, v3 center, v3 half, u32 surface) {
  if (c->count >= 16)
    return;
  EcsEntity e = ecs_entity_create(w);
  ecs_component_set(w, e, EntityName, {.name = fixedstr64_from_cstr((char *)name)});
  ecs_component_set(w, e, ColliderBody,
                    {.collider = collider_box(V3_ZERO, half),
                     .pos = center,
                     .rot = quat_identity(),
                     .scale = 1.0f,
                     .filter = {.category = COD_COLLIDE_WORLD, .mask = 0xFFFFFFFFu}});
  ecs_component_add(w, e, StaticBody);
  ecs_component_set(w, e, CodSurfaceTag, {.surface = surface});
  c->boxes[c->count++] = e;
}

// lanes along -Z from the origin: a thin 0.55 m vault, a 1.15 m mantle, a 3.2 m wall, a 1.8 m mantle, a 1.2 m ceiling slab, six stairs, a 0.4 m step
hz_internal void cod_course_spawn(EcsWorld *w, CodCourse *c) {
  cod_course_box(w, c, "CourseVault", (v3){-6.0f, 0.275f, -8.0f}, (v3){1.6f, 0.275f, 0.22f}, COD_SURFACE_WOOD);
  cod_course_box(w, c, "CourseMantle", (v3){0.0f, 0.575f, -8.0f}, (v3){1.6f, 0.575f, 1.6f}, COD_SURFACE_CONCRETE);
  cod_course_box(w, c, "CourseWall", (v3){6.0f, 1.6f, -8.0f}, (v3){1.6f, 1.6f, 0.8f}, COD_SURFACE_METAL);
  cod_course_box(w, c, "CourseMantleHigh", (v3){12.0f, 0.9f, -8.0f}, (v3){1.6f, 0.9f, 1.6f}, COD_SURFACE_METAL);
  cod_course_box(w, c, "CourseCeiling", (v3){-12.0f, 1.45f, -10.0f}, (v3){1.6f, 0.25f, 5.0f}, COD_SURFACE_CONCRETE);
  cod_course_box(w, c, "CourseStep", (v3){18.0f, 0.2f, -8.0f}, (v3){1.6f, 0.2f, 0.5f}, COD_SURFACE_DIRT);
  for (u32 s = 0; s < 6; s++) {
    f32 top = 0.18f * (f32)(s + 1);
    cod_course_box(w, c, "CourseStair", (v3){-18.0f, top * 0.5f, -5.0f - (f32)s * 0.3f}, (v3){1.5f, top * 0.5f, 0.15f}, COD_SURFACE_WOOD);
  }
  c->spawned = true;
}

ecs_system(cod_course_system, EcsIn(CodDevSwitches), EcsResInOut(CodCourse)) {
  CodCourse *c = ecs_sys_get_resource_mut(CodCourse);
  if (it->count <= 0)
    return;
  const CodDevSwitches *sw = ecs_sys_get_component(CodDevSwitches, 0);
  c->trace = sw->player_trace;
  if (sw->player_course && !c->spawned) {
    cod_course_spawn(it->world, c);
  } else if (!sw->player_course && c->spawned) {
    for (u32 k = 0; k < c->count; k++)
      ecs_entity_destroy(it->world, c->boxes[k]);
    c->count = 0;
    c->spawned = false;
  }
}

// ---- setup and schedule ----

hz_internal CharacterTuning cod_character_tuning(void) {
  CodStanceDef stand = cod_stance_def(COD_STANCE_STAND);
  return (CharacterTuning){
      .move_model = CHARACTER_MOVE_QUAKE,
      .facing = CHARACTER_FACING_HEADING,
      .radius = COD_PLAYER_RADIUS,
      .height = stand.height,
      .eye_height = stand.eye,
      .walk_speed = COD_PLAYER_WALK_SPEED,
      .run_speed = COD_PLAYER_SPRINT_SPEED,
      // the state machine owns the horizontal velocity and hands it over through add_velocity: ground control stays out of the way
      .friction = 1e-6f,
      .gravity = COD_GRAVITY,
      .max_fall_speed = 55.0f,
      .jump_height = COD_PLAYER_JUMP_APEX,
      // the jump request is ours (buffer, cooldown, stand-up check): the block takes it the step it is made
      .jump_buffer = 0.001f,
      .coyote_time = COD_MOVE_COYOTE_TIME,
      .slope_limit_deg = 48.0f,
      .step_height = stand.step_height,
      .snap_distance = 0.34f,
      .turn_rate = 30.0f,
  };
}

void cod_player_init(EcsWorld *w) {
  ecs_resource_set(w, CodPlayerState, ((CodPlayerState){0}));
  ecs_resource_set(w, CodPlayerAim, ((CodPlayerAim){.ads_fov_scale = COD_PLAYER_ADS_FOV_SCALE, .move_speed_scale = 1.0f}));
  ecs_resource_set(w, CodCourse, ((CodCourse){0}));
  character_controller_init(w);
  camera_rig_init(w);

  // the camera exists from the first frame (the app's init); the player's view takes it over once the player is placed
  EcsEntity camera = ECS_ENTITY_INVALID;
  EcsQuery *cams = ecs_query_cached(w, EcsIn(Camera));
  ecs_query_foreach(cams, qit) {
    if (qit.count > 0 && !camera)
      camera = qit.entities[0];
  }
  assert_msg(camera, "cod player: the camera entity must exist before cod_player_init");

  EcsEntity e = ecs_entity_create(w);
  ecs_component_set(w, e, EntityName, {.name = fixedstr64_from_cstr("Player")});
  ecs_component_set(w, e, CodPlayerIntake, {0});
  CodMove move = {.stance = COD_STANCE_STAND, .stance_want = COD_STANCE_STAND, .move_speed_scale = 1.0f, .grounded = true, .was_grounded = true};
  ecs_component_set(w, e, CodMove, move);
  CodHealth health;
  cod_health_reset(&health);
  ecs_component_set(w, e, CodHealth, health);
  ecs_component_set(w, e, CodView, cod_view_make(camera));
  ecs_component_set(w, e, WorldTransform, {.position = V3_ZERO, .rotation = quat_identity(), .scale = V3_ONE});
  ecs_component_set(w, e, LocalTransform, {.position = V3_ZERO, .rotation = quat_identity(), .scale = V3_ONE});
  ecs_component_set(w, e, CharacterTuning, cod_character_tuning());
  ecs_component_set(w, e, CharacterInput, {0});
  ecs_component_set(w, camera, CodCameraLink, {.player = e});
}

void cod_player_schedule_input(EcsWorld *w) {
  ecs_schedule_single(w, cod_course_system); // writes the CodCourse resource
  ecs_schedule(w, cod_player_input_system);
}

void cod_player_schedule_fixed(EcsWorld *w) {
  ecs_schedule(w, cod_move_pre_system);
  character_schedule_fixed(w);
  ecs_schedule(w, cod_move_post_system);
}

void cod_player_schedule(EcsWorld *w) {
  ecs_schedule_single(w, cod_player_alpha_system); // writes the CharacterFrame resource
  character_schedule_frame(w);
  ecs_schedule(w, cod_player_frame_system);
  ecs_schedule_single(w, cod_camera_publish_system); // writes the Camera, CameraRigState and CodPlayerState (one row)
}

void cod_player_schedule_intake(EcsWorld *w) {
  ecs_schedule(w, cod_player_intake_system);
  ecs_schedule_single(w, cod_player_consolidate); // writes the CodPlayerState resource (one row)
}
