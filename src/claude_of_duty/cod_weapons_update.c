// Claude of Duty weapons: the per-frame weapon system, the dev weapon switch, the fixed projectile step and the package entry points.

// ---- the dev weapon switch ----

hz_internal void cwp_set_weapon_immediate(CwpRig *r, u32 id) {
  if (id >= COD_WEAPON_COUNT)
    return;
  r->switch_to = -1;
  if (id != r->active)
    cwp_set_active(r, id, id != COD_WEAPON_PISTOL);
}

// ---- the frame ----

hz_internal u32 cwp_next_weapon(const CwpRig *r) { return (r->active + 1) % COD_WEAPON_COUNT; }

hz_internal void cwp_publish(CwpRig *r, CodWeaponState *out, b32 visible) {
  const CwpSlot *s = &r->slot[r->active];
  const CwpDef *def = cwp_def(r->active);
  out->weapon = r->active;
  out->fire_mode = s->mode;
  out->mag = s->mag;
  out->chambered = s->chambered;
  out->reserve = s->reserve;
  out->mag_size = def->mag_size;
  out->spread_deg = r->spread;
  out->ads = r->view.ads_t;
  out->reloading = cwp_reloading(r);
  out->firing = cwp_firing(r);
  out->switching = cwp_switching(r);
  out->inspecting = cwp_inspecting(r);
  out->muzzle = r->muzzle_world;
  out->muzzle_rot = r->bore_rot;
  out->visible = visible;
  out->reload_progress = r->reload_progress;
}

// the whole per-frame weapon logic; one serial state machine, the player's weapon is one logical instance
ecs_system(cwp_update_system, EcsResInOut(CodWeaponsRig), EcsResInOut(CodWeaponState), EcsResInOut(CodPlayerAim), EcsResIn(CodPlayerState),
           EcsResIn(EcsPhysicsWorld)) {
  CwpRig *r = ecs_sys_get_resource_mut(CodWeaponsRig)->rig;
  CodWeaponState *out = ecs_sys_get_resource_mut(CodWeaponState);
  CodPlayerAim *aim = ecs_sys_get_resource_mut(CodPlayerAim);
  const CodPlayerState *ps = ecs_sys_get_resource(CodPlayerState);
  EcsWorld *w = it->world;
  const CodClock *clock = ecs_resource_get(w, CodClock);
  const CodDev *dev = ecs_resource_get(w, CodDev);
  const CodInput *in = ecs_resource_get(w, CodInput);
  CodWeaponEvents *ev = &out->events;
  r->events = ev;
  if (!r->ready || !ps->ready) {
    out->visible = false;
    return;
  }
  f32 dt = clock->live ? clock->dt : 0.0f;
  // a respawn comes back with every weapon loaded: a dry rifle could never fight again
  if (ps->events.respawned) {
    for (u32 i = 0; i < COD_WEAPON_COUNT; i++) {
      const CwpDef *def = cwp_def(i);
      r->slot[i].mag = def->mag_size;
      r->slot[i].chambered = true;
      r->slot[i].reserve = def->reserve;
    }
  }
  r->cam_pos = ps->eye;
  r->cam_rot = ps->view_rot;
  r->cam_valid = true;
  r->player_velocity = ps->velocity;

  // dev switch: the weapon in hand
  if (dev->weapon >= 0 && dev->weapon != r->applied_weapon) {
    r->applied_weapon = dev->weapon;
    cwp_set_weapon_immediate(r, (u32)dev->weapon);
  }

  CwpSlot *s = &r->slot[r->active];
  const CwpDef *def = cwp_def(r->active);
  CwpInputs *st = &r->st;

  r->since_shot += dt;
  r->time += dt;
  if (r->fire_timer > 0.0f)
    r->fire_timer -= dt;
  if (r->burst_cooldown > 0.0f)
    r->burst_cooldown -= dt;

  // spread recovery
  f32 rest = cwp_rest_spread(r, def, ps);
  r->spread = m_maxf(rest, r->spread - def->spread_decay * dt * (1.0f + r->view.ads_t));
  if (r->since_shot > 0.6f)
    r->shot_index = 0;

  // gather the state the rig reads
  b32 live = clock->live && clock->controls && !ps->dead;
  b32 want_ads = live && (in->ads || ps->ads_requested);
  b32 held = live && in->fire;
  b32 pressed = live && in->fire_pressed;
  st->ads = want_ads;
  st->sprint = live && ps->sprinting && r->since_shot > 0.3f;
  st->speed = ps->speed;
  st->crouch = ps->stance == COD_STANCE_CROUCH;
  st->airborne = !ps->grounded;
  st->low_ready = ps->mantling;
  st->empty = s->mag == 0 && !s->chambered;
  if (want_ads != r->ads_was && live) {
    ev->ads_in = want_ads;
    ev->ads_out = !want_ads;
  }
  r->ads_was = want_ads;

  // the player's own jump and land kicks drive the gun's springs
  if (ps->events.jumped)
    cwp_view_jump(&r->view);
  if (ps->events.landed)
    cwp_view_land(&r->view, m_absf(ps->events.land_speed));

  if (live) {
    if (in->reload_pressed)
      cwp_reload(r);
    if (in->fire_mode_pressed)
      cwp_cycle_fire_mode(r, ev);
    if (in->inspect_pressed)
      cwp_inspect(r);
    if (in->weapon_slot_pressed >= 1 && in->weapon_slot_pressed <= 3)
      cwp_set_weapon(r, (u32)in->weapon_slot_pressed - 1u);
    if (in->swap_pressed || in->wheel)
      cwp_set_weapon(r, cwp_next_weapon(r));
    cwp_run_trigger(w, r, held, pressed);
    st->trigger = held && cwp_can_fire(r);
    if (pressed && st->empty)
      cwp_reload(r);
  }

  // the rig, now that the state is final
  CwpViewInput vin = {.ads = st->ads, .sprint = st->sprint, .low_ready = st->low_ready, .speed = st->speed, .airborne = st->airborne,
                      .trigger = st->trigger, .cam_rot = r->cam_rot};
  cwp_view_update(&r->view, &r->models[r->active], cwp_def(r->active), r->slot[r->active].clips, &vin, dt, cwp_on_clip_event, r);
  // a holster that ended inside the update changed the weapon
  const CwpModel *model = &r->models[r->active];

  r->muzzle_world = cwp_world_point(r, model->muzzle);
  r->bore_dir = cwp_world_dir(r, (v3){0.0f, 0.0f, -1.0f});
  r->bore_rot = quat_mul(r->cam_rot, r->view.rig_rot);
  cwp_scene_apply_arms(r, model);

  // reload progress for the HUD: 0..1 through the clip
  r->reload_progress = 0.0f;
  if (cwp_reloading(r)) {
    const CwpClip *c = &r->slot[r->active].clips[r->view.clip];
    r->reload_progress = c->duration > 0.0f ? m_minf(1.0f, r->view.clip_t / c->duration) : 0.0f;
  }

  // weapon:fire for every round that left this frame, with the bore
  for (u32 i = 0; i < r->pending_shots && ev->shot_count < COD_WEAPON_MAX_SHOTS; i++) {
    ev->shots[ev->shot_count++] = (CodShotEvent){.shooter = {.player = true},
                                                 .weapon = r->active,
                                                 .origin = r->muzzle_world,
                                                 .dir = r->bore_dir,
                                                 .seed = r->fire_seed,
                                                 .viewmodel = true,
                                                 .view_muzzle = r->muzzle_world};
  }
  r->pending_shots = 0;

  // deferred shell ejection
  for (u32 i = 0; i < CWP_SHELL_SLOTS; i++) {
    if (r->shell_t[i] < 0.0f)
      continue;
    r->shell_t[i] -= dt;
    if (r->shell_t[i] > 0.0f)
      continue;
    r->shell_t[i] = -1.0f;
    if (ev->eject_count >= COD_WEAPON_MAX_EJECTS)
      continue;
    quat wq = r->bore_rot;
    f32 speed = 2.3f + cod_rng_float(&r->rng) * 1.2f;
    v3 vel = v3_scale(v3_normalize(quat_rotate(wq, model->eject_dir)), speed);
    vel = v3_add(vel, r->player_velocity);
    vel.y += 1.1f;
    f32 spin = 28.0f + cod_rng_float(&r->rng) * 34.0f;
    f32 sx = cod_rng_signed(&r->rng), sy = cod_rng_signed(&r->rng), sz = cod_rng_signed(&r->rng);
    ev->ejects[ev->eject_count++] = (CodEjectEvent){.position = cwp_world_point(r, model->eject),
                                                    .velocity = vel,
                                                    .rotation = wq,
                                                    .spin = {sx * spin, sy * spin * 0.7f, sz * spin},
                                                    .weapon = r->active,
                                                    .magazine = false,
                                                    .case_len = model->case_len,
                                                    .case_radius = model->rim_r};
  }

  // the camera climb the shots earned goes to the player's inbox
  CodPlayerIntake *intake = ecs_component_get_mut(w, ps->entity, CodPlayerIntake);
  if (intake) {
    intake->recoil_pitch += r->recoil_pitch;
    intake->recoil_yaw += r->recoil_yaw;
    intake->recoil_roll += r->recoil_roll;
    intake->recoil_punch += r->recoil_punch;
  }
  r->recoil_pitch = r->recoil_yaw = r->recoil_roll = r->recoil_punch = 0.0f;
  cwp_mag_retire(r, dt);

  aim->driven = true;
  aim->ads = r->view.ads_t;
  cwp_publish(r, out, true);
  if (dev->weapon_trace)
    log_info("weapon t=% id=% mode=% mag=% ch=% res=% ads=% rel=% relp=% fire=% sw=% insp=% shots=% ev_rel=% ev_mode=% ev_swap=% ev_dry=%",
             fmt_f32_p(clock->time, 3), fmt_u32(out->weapon), fmt_u32(out->fire_mode), fmt_u32(out->mag), fmt_b32(out->chambered),
             fmt_u32(out->reserve), fmt_f32_p(out->ads, 2), fmt_b32(out->reloading), fmt_f32_p(out->reload_progress, 2),
             fmt_b32(out->firing), fmt_b32(out->switching), fmt_b32(out->inspecting), fmt_u32(out->events.shot_count),
             fmt_b32(out->events.reload), fmt_b32(out->events.fire_mode_changed), fmt_b32(out->events.swapped),
             fmt_b32(out->events.dry_fire));
  r->events = NULL;
}

// the fixed step: every round in flight advances and, on contact, goes through the penetration solver
ecs_system(cwp_fixed_system, EcsResInOut(CodWeaponsRig), EcsResInOut(CodWeaponState), EcsResIn(CodPlayerState), EcsResIn(EcsPhysicsWorld)) {
  CwpRig *r = ecs_sys_get_resource_mut(CodWeaponsRig)->rig;
  if (!r->ready || r->live_count == 0)
    return;
  r->events = &ecs_sys_get_resource_mut(CodWeaponState)->events;
  cwp_step_projectiles(it->world, r, it->delta_time);
  r->events = NULL;
}

// ---- entry points ----

void cod_weapons_init(EcsWorld *w) {
  ecs_resource_set_zero(w, CodWeaponState);
  ecs_resource_set_zero(w, CodExplosions);
  Allocator persist = make_arena_allocator(arena_create(MB(8), KB(256)));
  CwpRig *r = ALLOC(&persist, CwpRig);
  r->world = w;
  r->models = ALLOC_ARRAY(&persist, CwpModel, COD_WEAPON_COUNT);
  r->arm = ALLOC_ARRAY(&persist, CwpArm, 2);
  for (u32 i = 0; i < 5; i++)
    r->arena[i] = arena_create(GB(1), MB(8));
  r->rng = cod_rng_make(0x57e40001u);
  cwp_view_init(&r->view, cod_rng_fork(&r->rng));
  for (u32 i = 0; i < COD_WEAPON_COUNT; i++) {
    const CwpDef *def = cwp_def(i);
    CwpSlot *s = &r->slot[i];
    s->mag = def->mag_size;
    s->chambered = true;
    s->reserve = def->reserve;
    s->mode = def->modes[0];
    cwp_build_recoil_pattern(def, s->pattern);
  }
  r->active = COD_WEAPON_RIFLE;
  r->switch_to = -1;
  r->since_shot = 10.0f;
  r->applied_weapon = -1;
  for (u32 i = 0; i < CWP_SHELL_SLOTS; i++)
    r->shell_t[i] = -1.0f;
  r->view.hand_pose_rest = CWP_POSE_FITTED + COD_WEAPON_RIFLE;
  r->persist = persist;
  r->scene = cwp_scene_make(w, r);
  ecs_resource_set(w, CodWeaponsRig, ((CodWeaponsRig){.rig = r}));
  cwp_grenade_init(w);
  r->stage_t0 = os_time_now();
}

void cod_weapons_schedule(EcsWorld *w) {
  ecs_schedule_single(w, cwp_update_system);  // one logical player weapon: a serial state machine writing the shared state, inbox and aim
  ecs_schedule_single(w, cwp_grenade_system); // owns the CodExplosions list and pushes the blast through the one-caller impulse buffer
  ecs_schedule_single(w, cwp_load_system);    // writes the CodLoad resource
}

void cod_weapons_schedule_fixed(EcsWorld *w) {
  ecs_schedule_single(w, cwp_fixed_system); // rounds in flight share one list and push rigid-body impulses through a one-caller buffer
}

void cod_weapons_schedule_view(EcsWorld *w) {
  ecs_schedule(w, cwp_place_system);  // per mesh entity, parallel: the gun, parts, arms and reticle on the final camera
  ecs_schedule(w, cwp_follow_system); // per visual entity, parallel: dropped magazines and grenades follow their bodies
}

void cod_weapons_lanes(EcsWorld *w) {
  CwpRig *r = ecs_resource_get(w, CodWeaponsRig)->rig;
  if (r->stage != CWP_STAGE_BUILD)
    return;
  cwp_scene_build_lanes(r);
}

void cod_weapons_frame(EcsWorld *w) {
  CwpRig *r = ecs_resource_get(w, CodWeaponsRig)->rig;
  // this frame's events were consumed inside the scope
  ecs_resource_get_mut(w, CodWeaponState)->events = (CodWeaponEvents){0};
  ecs_resource_get_mut(w, CodExplosions)->count = 0;
  cwp_scene_frame(w, r);
}
