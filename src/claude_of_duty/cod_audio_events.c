// Claude of Duty audio, event consumers: one consolidation system per producer and the ambience scheduler.

typedef enum CasBarkKind {
  CAS_BK_SPOT = 0,
  CAS_BK_RELOAD,
  CAS_BK_GRENADE,
  CAS_BK_FLANK,
  CAS_BK_SUPPRESS,
  CAS_BK_ADVANCE,
  CAS_BK_HURT,
  CAS_BK_DEATH,
  CAS_BK_COPY,
  CAS_BK_COUNT,
} CasBarkKind;

hz_internal CodAudio *cas_get(const CodAudioState *st) { return st->impl; }

hz_internal b32 cas_on(const CodAudio *a) { return a && a->sources_made && a->active; }

hz_internal u32 cas_profile_for_weapon(u32 weapon) { return weapon == COD_WEAPON_SMG ? CAS_PROF_SMG : (weapon == COD_WEAPON_PISTOL ? CAS_PROF_PISTOL : CAS_PROF_RIFLE); }

hz_internal u32 cas_bark_for(CodAudio *a, u32 kind) {
  switch (kind) {
  case CAS_BK_SPOT: return cod_rng_float(&a->rng) < 0.5f ? CAS_BARK_CONTACT : CAS_BARK_SPOTTED;
  case CAS_BK_RELOAD: return CAS_BARK_RELOADING;
  case CAS_BK_GRENADE: return CAS_BARK_GRENADE;
  case CAS_BK_FLANK: return CAS_BARK_FLANKING;
  case CAS_BK_SUPPRESS: return CAS_BARK_SUPPRESSING;
  case CAS_BK_ADVANCE: return CAS_BARK_MOVEUP;
  case CAS_BK_HURT: return cod_rng_float(&a->rng) < 0.5f ? CAS_BARK_HIT : CAS_BARK_PAIN;
  case CAS_BK_DEATH: return CAS_BARK_DEATH;
  case CAS_BK_COPY: return CAS_BARK_COPY;
  default: return CAS_BARK_CONTACT;
  }
}

// no mush (0.42 s between barks unless forced), a voice per speaker, positional or head-locked
hz_internal b32 cas_bark(CodAudio *a, u32 kind, b32 positional, v3 pos, f32 level, u32 voice, b32 force, b32 radio, f32 dry_send) {
  if (a->clock - a->last_bark < 0.42 && !force)
    return false;
  a->last_bark = a->clock;
  const u32 bark = cas_bark_for(a, kind);
  const u32 preset = radio ? 0u : voice % CAS_VOICE_PRESETS;
  const u32 set = a->base[CAS_K_BARK] + (bark * CAS_VOICE_PRESETS + preset) * 2u + (radio ? 1u : 0u);
  if (positional)
    return cas_play_at(a, &(CasPlay){.set = set, .pos = pos, .level = level, .send = -1.0f, .occlusion = -1.0f});
  cas_play_dry(a, set, level, dry_send, 1.0f, 0);
  return true;
}

/* ------------------------------------------------------------ the space probe */

hz_internal void cas_probe_space(CodAudio *a) {
  f32 hits[CAS_PROBE_RAYS] = {0};
  for (u32 i = 0; i < CAS_PROBE_RAYS; i++) {
    v3 dir = {0.0f, 1.0f, 0.0f};
    if (i < CAS_PROBE_RAYS - 1u) {
      const f32 ang = (f32)i / (f32)(CAS_PROBE_RAYS - 1u) * 2.0f * M_PI;
      dir = v3_normalize((v3){m_cosf(ang), 0.06f, m_sinf(ang)});
    }
    f32 hit = 0.0f;
    const v3 end = v3_add(a->listener_pos, v3_scale(dir, CAS_PROBE_DIST));
    hits[i] = cas_ray_world(a, a->listener_pos, end, &hit) ? hit : CAS_PROBE_DIST;
  }
  a->space = cas_classify_space(hits);
  for (u32 k = 0; k < CAS_SPACES; k++)
    a->conv->target[k] = a->space.w[k];
  cas_bed_enclosure(&a->bed, a->space.enclosure);
}

/* ------------------------------------------------------------ weapon, impact and tracer voices */

// the own gun is head-locked and mostly dry; the room answers through the send
hz_internal void cas_shot_dry(CodAudio *a, u32 profile) {
  const f32 *w = a->space.w;
  const f32 echo = 0.35f + w[CAS_SPACE_TIGHT] * 0.5f + w[CAS_SPACE_STREET] * 0.9f + w[CAS_SPACE_TUNNEL] * 1.0f + w[CAS_SPACE_ROOM] * 0.75f;
  const f32 boost = 0.75f + w[CAS_SPACE_STREET] * 0.7f + w[CAS_SPACE_TIGHT] * 0.35f + w[CAS_SPACE_TUNNEL] * 0.8f + w[CAS_SPACE_OPEN] * 0.2f;
  cas_play_dry(a, cas_shot_set(a, profile, 0.0f, true), 1.0f, echo * 0.6f * boost, cas_pitch(a, 1.0f, 0.02f), 0);
  cas_duck(a, 0.55f, 0.1f);
}

hz_internal void cas_shot_at(CodAudio *a, u32 profile, v3 pos, f32 gain, f32 occlusion, f32 extra_delay, f32 max_dist) {
  const f32 *w = a->space.w;
  const f32 d = v3_distance(pos, a->listener_pos);
  const f32 far = 1.0f - cas_clamp(1.0f - d / 42.0f, 0.0f, 1.0f);
  const f32 boost = 0.75f + w[CAS_SPACE_STREET] * 0.7f + w[CAS_SPACE_TIGHT] * 0.35f + w[CAS_SPACE_TUNNEL] * 0.8f + w[CAS_SPACE_OPEN] * 0.2f;
  cas_play_at(a, &(CasPlay){.set = cas_shot_set(a, profile, d, false),
                            .pos = pos,
                            .gain = gain,
                            .send = CAS_PROFILES[profile].send * (1.0f + far * 1.4f) * boost,
                            .pitch_jitter = 0.02f,
                            .extra_delay = extra_delay,
                            .occlusion = occlusion,
                            .max_dist = max_dist,
                            .baked_air = true});
}

// a shot from anyone: the player's own gun (a muzzle at the ear), or somebody else's
hz_internal void cas_on_fire(CodAudio *a, const CodShotEvent *s, b32 viewmodel, u32 soldier) {
  const u32 profile = cas_profile_for_weapon(s->weapon);
  const v3 origin = viewmodel ? s->view_muzzle : s->origin;
  const f32 dist = v3_distance(origin, a->listener_pos);
  if (viewmodel || dist < 2.6f) {
    cas_shot_dry(a, profile);
    return;
  }
  cas_shot_at(a, profile, origin, 1.0f, -1.0f, 0.0f, 0.0f);
  cas_duck(a, cas_clamp(0.5f - dist * 0.004f, 0.12f, 0.5f), 0.08f);
  if (a->clock - a->last_enemy_fire > 4.5 && cod_rng_float(&a->rng) < 0.45f) {
    a->last_enemy_fire = a->clock;
    cas_bark(a, cod_rng_float(&a->rng) < 0.6f ? CAS_BK_SPOT : CAS_BK_SUPPRESS, true, origin, 0.9f, soldier, false, false, 0.25f);
  }
}

hz_internal u32 cas_surface_below(const CodAudio *a, v3 from, v3 to) {
  const EcsCollisionLayer *layer = ecs_physics_get_static_layer(a->world);
  PhysicsRaycastResult r = {0};
  i32 idx = 0;
  if (layer && physics_raycast_layer(from, to, (CollisionLayer *)&layer->layer, PHYSICS_QUERY_FILTER_ALL, &r, &idx))
    return cod_entity_surface(a->world, layer->entities[idx]);
  return COD_SURFACE_CONCRETE;
}

// weapon:shell: brass finds what it will land on, rings after its flight
hz_internal void cas_on_shell(CodAudio *a, v3 pos) {
  if (a->budget_shell++ > 2)
    return;
  const f32 dist = v3_distance(pos, a->listener_pos);
  if (dist > 22.0f)
    return;
  const u32 surface = cas_surface_below(a, (v3){pos.x, pos.y + 0.2f, pos.z}, (v3){pos.x, pos.y + 0.2f - 6.0f, pos.z});
  cas_play_at(a, &(CasPlay){.set = a->base[CAS_K_SHELL] + surface,
                            .pos = (v3){pos.x, pos.y - 0.6f, pos.z},
                            .level = cas_clamp(1.0f - dist * 0.02f, 0.3f, 1.0f),
                            .send = -1.0f,
                            .extra_delay = 0.25f + cod_rng_float(&a->rng) * 0.3f,
                            .occlusion = -1.0f});
}

hz_internal u32 cas_whizz_set(const CodAudio *a, f32 miss) {
  u32 best = 0;
  f32 best_d = 1e9f;
  for (u32 m = 0; m < CAS_WHIZZ_COUNT; m++) {
    const f32 d = m_absf(miss - CAS_WHIZZ_MISS[m]);
    if (d < best_d) {
      best_d = d;
      best = m;
    }
  }
  return a->base[CAS_K_WHIZZ] + best;
}

// bullet:impact and bullet:tracer for one round
hz_internal void cas_on_round(CodAudio *a, const CodRound *r) {
  for (u32 i = 0; i < r->impact_count && i < COD_BALLISTICS_MAX_LAYERS * 2u; i++) {
    const CodImpact *im = &r->impacts[i];
    if (im->exit)
      continue;
    if (a->budget_impact++ > 4)
      continue;
    const f32 dist = v3_distance(im->point, a->listener_pos);
    if (dist > 90.0f)
      continue;
    cas_play_at(a, &(CasPlay){.set = a->base[CAS_K_IMPACT] + (im->surface < CAS_SURFACES ? im->surface : 0u),
                              .pos = im->point,
                              .level = cas_clamp(im->damage / 34.0f, 0.35f, 1.5f),
                              .send = -1.0f,
                              .occlusion = -1.0f});
    if (dist < 6.0f)
      cas_play_at(a, &(CasPlay){.set = cas_whizz_set(a, dist), .pos = im->point, .send = -1.0f, .no_delay = true, .occlusion = -1.0f});
  }
  if (!r->has_tracer || a->budget_whizz++ > 2)
    return;
  const v3 from = r->tracer.from, to = r->tracer.to, lp = a->listener_pos;
  const v3 d = v3_sub(to, from);
  const f32 len2 = v3_dot(d, d);
  if (len2 < 1e-6f)
    return;
  const f32 t = cas_clamp(v3_dot(v3_sub(lp, from), d) / len2, 0.0f, 1.0f);
  const v3 closest = v3_add(from, v3_scale(d, t));
  const f32 miss = v3_distance(lp, closest);
  if (miss > 5.0f || v3_distance(lp, from) < 3.0f)
    return;
  const f32 flight = m_sqrtf(len2) * t / (r->tracer.speed > 0.0f ? r->tracer.speed : 850.0f);
  cas_play_at(a, &(CasPlay){.set = cas_whizz_set(a, miss), .pos = closest, .send = -1.0f, .no_delay = true, .extra_delay = flight, .occlusion = -1.0f});
}

hz_internal void cas_ui(CodAudio *a, u32 kind, f32 level, f32 send_level) {
  cas_play_dry(a, a->base[CAS_K_UI] + kind, level, send_level, 1.0f, 0);
}

/* ------------------------------------------------------------ systems */

ecs_system(cod_audio_load_system, EcsResInOut(CodAudioState), EcsResInOut(CodLoad)) {
  CodAudioState *st = ecs_sys_get_resource_mut(CodAudioState);
  CodAudio *a = st->impl;
  f32 p = 1.0f;
  if (a && !st->ready)
    p = 0.97f * (f32)a->job_done / (f32)(a->job_count ? a->job_count : 1u);
  st->progress = p;
  ecs_sys_get_resource_mut(CodLoad)->progress[COD_LOAD_AUDIO] = p;
}

// the dev switch: meter the buses
ecs_system(cod_audio_dev_system, EcsIn(CodDevSwitches), EcsResInOut(CodAudioState)) {
  CodAudio *a = ecs_sys_get_resource_mut(CodAudioState)->impl;
  if (!a || it->count == 0)
    return;
  a->meter = ecs_sys_get_component(CodDevSwitches, 0)->audio_meter;
}

ecs_system(cod_audio_begin_system, EcsResInOut(CodAudioState), EcsResIn(CodFlow), EcsResIn(CodSettings)) {
  CodAudio *a = ecs_sys_get_resource_mut(CodAudioState)->impl;
  if (!a || !a->sources_made)
    return;
  const CodFlow *flow = ecs_sys_get_resource(CodFlow);
  a->active = (flow->state == COD_STATE_PLAYING && !flow->audio_suspended);
  a->suspended = flow->audio_suspended;
  a->volume = ecs_sys_get_resource(CodSettings)->volume;
  a->master.gain_target = a->volume * CAS_MASTER_VOLUME;
  a->budget_impact = a->budget_step = a->budget_shell = a->budget_whizz = 0;
  a->frame++;
}

// the listener is the camera
ecs_system(cod_audio_listener_system, EcsIn(Camera), EcsResInOut(CodAudioState)) {
  CodAudio *a = ecs_sys_get_resource_mut(CodAudioState)->impl;
  if (!a || it->count == 0)
    return;
  const Camera *cam = ecs_sys_get_component(Camera, 0);
  a->listener_pos = cam->pos;
  a->listener_fwd = quat_rotate(cam->rot, V3_BACK);
  a->listener_up = quat_rotate(cam->rot, V3_UP);
}

// nine rays every 0.45 s or 1.6 m
ecs_system(cod_audio_space_system, EcsResInOut(CodAudioState), EcsResIn(EcsPhysicsWorld)) {
  CodAudio *a = ecs_sys_get_resource_mut(CodAudioState)->impl;
  if (!cas_on(a))
    return;
  const f32 dt = ecs_resource_get(it->world, AppMemory)->dt;
  a->probe_timer -= dt;
  const v3 lp = a->listener_pos;
  const f32 moved = m_absf(lp.x - a->probe_pos.x) + m_absf(lp.y - a->probe_pos.y) + m_absf(lp.z - a->probe_pos.z);
  if (a->probe_timer <= 0.0f || moved > 1.6f || !a->probed) {
    a->probe_timer = 0.45f;
    a->probe_pos = lp;
    a->probed = true;
    cas_probe_space(a);
  }
}

// the player's weapon, movement, health and the frame's explosions
ecs_system(cod_audio_player_system, EcsResInOut(CodAudioState), EcsResIn(CodPlayerState), EcsResIn(CodWeaponState), EcsResIn(CodExplosions),
           EcsResIn(CodHudModel), EcsResIn(CodClock), EcsResIn(EcsPhysicsWorld)) {
  CodAudio *a = ecs_sys_get_resource_mut(CodAudioState)->impl;
  if (!cas_on(a))
    return;
  const CodPlayerState *ps = ecs_sys_get_resource(CodPlayerState);
  const CodWeaponEvents *we = &ecs_sys_get_resource(CodWeaponState)->events;
  const CodPlayerEvents *pe = &ps->events;
  const f32 dt = ecs_sys_get_resource(CodClock)->dt;

  for (u32 i = 0; i < we->shot_count && i < COD_WEAPON_MAX_SHOTS; i++)
    cas_on_fire(a, &we->shots[i], we->shots[i].viewmodel, 0);
  if (we->dry_fire)
    cas_play_dry(a, a->base[CAS_K_DRYFIRE], 1.0f, 0.15f, 1.0f, 0);
  if (we->reload)
    cas_play_dry(a, a->base[CAS_K_RELOAD] + (we->reload_phase < 4u ? we->reload_phase : 3u), 1.0f, 0.22f, 1.0f, 0);
  for (u32 i = 0; i < we->eject_count && i < COD_WEAPON_MAX_EJECTS; i++)
    if (!we->ejects[i].magazine)
      cas_on_shell(a, we->ejects[i].position);
  for (u32 i = 0; i < we->round_count && i < COD_WEAPON_MAX_ROUNDS; i++)
    cas_on_round(a, &we->rounds[i]);

  // player:state: the stance and the aim sound as cloth
  if (ps->ready && ps->stance != a->stance_seen) {
    a->stance_seen = ps->stance;
    cas_play_dry(a, a->base[CAS_K_CLOTH], 0.9f, 0.12f, 1.0f, 0);
  }
  if (we->ads_in || we->ads_out)
    cas_play_dry(a, a->base[CAS_K_CLOTH], 0.45f, 0.1f, 1.0f, 0);

  // player:footstep and player:land
  if (pe->footstep && a->budget_step++ <= 3) {
    const f32 dist = v3_distance(pe->footstep_position, a->listener_pos);
    if (dist <= 45.0f) {
      const u32 gait = pe->footstep_running ? CAS_GAIT_RUN : ((pe->footstep_stance == COD_STANCE_CROUCH || pe->footstep_stance == COD_STANCE_PRONE) ? CAS_GAIT_CROUCH : CAS_GAIT_WALK);
      cas_play_at(a, &(CasPlay){.set = a->base[CAS_K_STEP] + (pe->footstep_surface < CAS_SURFACES ? pe->footstep_surface : 0u) * CAS_GAIT_COUNT + gait,
                                .pos = pe->footstep_position,
                                .level = dist < 2.0f ? 0.72f : 1.0f,
                                .send = -1.0f,
                                .pitch_jitter = 0.03f,
                                .occlusion = -1.0f});
    }
  }
  if (pe->landed) {
    const f32 v = m_absf(pe->land_speed);
    cas_play_at(a, &(CasPlay){.set = a->base[CAS_K_STEP] + (pe->land_surface < CAS_SURFACES ? pe->land_surface : 0u) * CAS_GAIT_COUNT + CAS_GAIT_LAND,
                              .pos = (v3){a->listener_pos.x, a->listener_pos.y - 1.6f, a->listener_pos.z},
                              .level = cas_clamp(v / 7.0f, 0.35f, 1.7f),
                              .send = -1.0f,
                              .occlusion = -1.0f});
    if (v > 8.5f)
      cas_play_dry(a, a->base[CAS_K_CLOTH], 0.8f, 0.15f, 1.0f, 0);
  }

  // damage:taken, then the HUD's own cues
  for (u32 i = 0; i < pe->hit_count && i < COD_PLAYER_MAX_HITS; i++) {
    const f32 amount = pe->hits[i].amount;
    cas_ui(a, CAS_UI_DAMAGE, cas_clamp(amount / 25.0f, 0.4f, 1.4f), 0.0f);
    if (amount > 12.0f && cod_rng_float(&a->rng) < 0.5f)
      cas_bark(a, CAS_BK_HURT, false, V3_ZERO, 0.5f, 1, true, false, 0.1f);
  }
  const CodHudModel *hud = ecs_sys_get_resource(CodHudModel);
  for (u32 i = 0; i < hud->hit_count && i < COD_HUD_EVENT_HITS; i++) {
    const u32 kind = hud->hits[i];
    if (kind == COD_HIT_KILL)
      cas_ui(a, CAS_UI_KILL, 1.0f, 0.0f);
    else if (kind == COD_HIT_HEAD)
      cas_ui(a, CAS_UI_HEADSHOT, 0.7f, 0.0f);
    else if (kind == COD_HIT_ARMOUR)
      cas_ui(a, CAS_UI_ARMOUR, 0.7f, 0.0f);
    else
      cas_ui(a, CAS_UI_HITMARKER, 0.7f, 0.0f);
  }
  for (u32 i = 0; i < hud->hurt_count && i < COD_HUD_EVENT_HURTS; i++)
    if (hud->hurts[i].damage)
      cas_ui(a, CAS_UI_DAMAGE, 0.6f + cas_clamp(hud->hurts[i].amount / 40.0f, 0.0f, 1.0f) * 0.4f, 0.0f);
  if (ps->regenerating && !a->regen_seen)
    cas_ui(a, CAS_UI_REGEN, 0.4f, 0.0f);
  a->regen_seen = ps->regenerating;
  if (pe->heartbeat)
    cas_play_dry(a, a->base[CAS_K_HEARTBEAT], 0.35f + pe->heartbeat_strength * 0.5f, 0.1f, 1.0f, CAS_BUS_UI);

  // the low-health heartbeat on the foley bus
  a->health = ps->health;
  if (ps->ready && !ps->dead && a->health < 34.0f) {
    a->heart_timer -= dt;
    if (a->heart_timer <= 0.0f) {
      a->heart_timer = 0.62f + (a->health / 34.0f) * 0.45f;
      cas_play_dry(a, a->base[CAS_K_HEARTBEAT], cas_clamp(1.0f - a->health / 34.0f, 0.2f, 1.0f), 0.1f, 1.0f, 0);
    }
  }

  // explosion: the blast, the duck, the concussion
  const CodExplosions *ex = ecs_sys_get_resource(CodExplosions);
  for (u32 i = 0; i < ex->count && i < COD_MAX_EXPLOSIONS; i++) {
    const CodExplosionEvent *e = &ex->items[i];
    const f32 radius = e->radius > 0.0f ? e->radius : 6.0f;
    const f32 dist = v3_distance(e->position, a->listener_pos);
    cas_play_at(a, &(CasPlay){.set = cas_explosion_set(a, radius, dist),
                              .pos = e->position,
                              .level = e->flash ? 0.6f : 1.0f,
                              .send = 1.0f,
                              .occlusion = -1.0f,
                              .baked_air = true});
    cas_duck(a, 0.85f, 0.35f);
    const f32 near_v = cas_clamp(1.0f - dist / 22.0f, 0.0f, 1.0f);
    if (near_v > 0.1f)
      cas_concuss(a, m_powf(near_v, 1.4f));
  }
}

// the soldiers: their shots, brass, rounds, footsteps, callouts and deaths, and what the player's rounds did to them
ecs_system(cod_audio_soldier_system, EcsIn(CodSoldier), EcsIn(CodSoldierEventsComp), EcsResInOut(CodAudioState), EcsResIn(CodWeaponState),
           EcsResIn(CodClock), EcsResIn(EcsPhysicsWorld)) {
  CodAudio *a = ecs_sys_get_resource_mut(CodAudioState)->impl;
  if (!cas_on(a))
    return;
  const CodWeaponEvents *we = &ecs_sys_get_resource(CodWeaponState)->events;
  const CodClock *clock = ecs_sys_get_resource(CodClock);
  for (i32 i = 0; i < it->count; i++) {
    const CodSoldier *s = ecs_sys_get_component(CodSoldier, i);
    const CodSoldierEvents *ev = &ecs_sys_get_component(CodSoldierEventsComp, i)->ev;
    const u32 id = (u32)it->entities[i];
    const b32 fresh = ev->frame == clock->frame;
    if (fresh && ev->fired) {
      cas_on_fire(a, &ev->shot, false, id);
      cas_on_round(a, &ev->round);
    }
    if (fresh && ev->ejected)
      cas_on_shell(a, ev->eject.position);
    if (fresh && ev->footstep && a->budget_step++ <= 3) {
      const f32 dist = v3_distance(s->position, a->listener_pos);
      if (dist <= 45.0f)
        cas_play_at(a, &(CasPlay){.set = a->base[CAS_K_STEP] + (ev->footstep_surface < CAS_SURFACES ? ev->footstep_surface : 0u) * CAS_GAIT_COUNT + CAS_GAIT_WALK,
                                  .pos = s->position,
                                  .send = -1.0f,
                                  .pitch_jitter = 0.03f,
                                  .occlusion = -1.0f});
    }
    if (fresh && ev->voice)
      cas_bark(a, ev->voice_line % CAS_BK_COUNT, true, s->head, 1.0f, id, false, false, 0.25f);

    // damage:dealt to this soldier by the player's rounds: the hit tick, the kill tick, a grunt now and then
    u32 last = ~0u;
    for (u32 r = 0; r < we->round_count && r < COD_WEAPON_MAX_ROUNDS; r++) {
      const CodRound *round = &we->rounds[r];
      if (round->shooter.player && round->hit_soldier == it->entities[i] && round->hit_damage > 0.0f)
        last = r;
    }
    const b32 killed_by_player = fresh && ev->killed && ev->killed_by_player;
    for (u32 r = 0; r < we->round_count && r < COD_WEAPON_MAX_ROUNDS; r++) {
      const CodRound *round = &we->rounds[r];
      if (!round->shooter.player || round->hit_soldier != it->entities[i] || round->hit_damage <= 0.0f)
        continue;
      cas_ui(a, round->hit_zone == COD_ZONE_HEAD ? CAS_UI_HEADSHOT : CAS_UI_HITMARKER, 1.0f, 0.0f);
      if (killed_by_player && r == last)
        cas_ui(a, CAS_UI_KILL, 1.0f, 0.0f);
      else if (cod_rng_float(&a->rng) < 0.3f)
        cas_bark(a, CAS_BK_HURT, true, round->hit_point, 0.85f, id, false, false, 0.25f);
    }

    // actor:death: the death cry, then the body hits the ground
    if (fresh && ev->killed) {
      cas_bark(a, CAS_BK_DEATH, true, ev->death_point, 1.0f, id, true, false, 0.25f);
      cas_play_at(a, &(CasPlay){.set = a->base[CAS_K_BODYFALL], .pos = ev->death_point, .send = -1.0f,
                                .extra_delay = 0.45f + cod_rng_float(&a->rng) * 0.4f, .occlusion = -1.0f});
    }
  }
}

// distant volleys, booms, one-shots and chatter on timers
ecs_system(cod_audio_ambience_system, EcsResInOut(CodAudioState), EcsResIn(CodClock), EcsResIn(EcsPhysicsWorld)) {
  CodAudio *a = ecs_sys_get_resource_mut(CodAudioState)->impl;
  if (!cas_on(a))
    return;
  const f32 dt = ecs_sys_get_resource(CodClock)->dt;
  CodRng *r = &a->rng;
  const v3 lp = a->listener_pos;
  a->t_volley -= dt;
  if (a->t_volley <= 0.0f) {
    a->t_volley = cod_rng_range(r, 2.5f, 12.0f);
    const f32 ang = cod_rng_range(r, 0.0f, 2.0f * M_PI), d = cod_rng_range(r, 70.0f, 240.0f);
    const v3 at = {lp.x + m_cosf(ang) * d, lp.y + cod_rng_range(r, -2.0f, 6.0f), lp.z + m_sinf(ang) * d};
    const u32 pick = cod_rng_pick(r, 4);
    const u32 profile = pick == 0 ? CAS_PROF_AK : (pick == 1 ? CAS_PROF_RIFLE : (pick == 2 ? CAS_PROF_LMG : CAS_PROF_SNIPER));
    const u32 rounds = 1u + (cod_rng_u32(r) % 6u);
    const f32 rate = cod_rng_range(r, 0.075f, 0.13f);
    for (u32 k = 0; k < rounds; k++)
      cas_shot_at(a, profile, at, 4.5f, 0.0f, (f32)k * rate * cod_rng_range(r, 0.9f, 1.1f), 400.0f);
  }
  a->t_boom -= dt;
  if (a->t_boom <= 0.0f) {
    a->t_boom = cod_rng_range(r, 16.0f, 50.0f);
    const f32 ang = cod_rng_range(r, 0.0f, 2.0f * M_PI), d = cod_rng_range(r, 120.0f, 330.0f);
    const v3 at = {lp.x + m_cosf(ang) * d, lp.y + cod_rng_range(r, 0.0f, 8.0f), lp.z + m_sinf(ang) * d};
    const f32 radius = cod_rng_range(r, 6.0f, 16.0f);
    cas_play_at(a, &(CasPlay){.set = cas_explosion_set(a, radius, v3_distance(at, lp)), .pos = at, .gain = 6.0f, .send = -1.0f, .max_dist = 400.0f,
                              .occlusion = 0.0f, .baked_air = true});
  }
  a->t_oneshot -= dt;
  if (a->t_oneshot <= 0.0f) {
    a->t_oneshot = cod_rng_range(r, 6.0f, 20.0f);
    const u32 kind = cod_rng_pick(r, CAS_AMB_COUNT);
    const b32 far = cas_ambient_far(kind);
    const f32 d = far ? cod_rng_range(r, 90.0f, 260.0f) : cod_rng_range(r, 14.0f, 90.0f);
    const f32 ang = cod_rng_range(r, 0.0f, 2.0f * M_PI);
    const v3 at = {lp.x + m_cosf(ang) * d, lp.y + cod_rng_range(r, -1.0f, kind == CAS_AMB_HELI ? 28.0f : 5.0f), lp.z + m_sinf(ang) * d};
    const u32 cls = (!far && d >= 45.0f) ? 1u : 0u;
    cas_play_at(a, &(CasPlay){.set = a->base[CAS_K_AMBIENT] + kind * 2u + cls, .pos = at, .level = cod_rng_range(r, 0.55f, 1.0f),
                              .gain = far ? 14.0f : 2.5f, .send = -1.0f, .max_dist = 400.0f, .occlusion = far ? 0.0f : -1.0f, .baked_air = true});
  }
  a->t_chatter -= dt;
  if (a->t_chatter <= 0.0f) {
    a->t_chatter = cod_rng_range(r, 20.0f, 60.0f);
    const f32 ang = cod_rng_range(r, 0.0f, 2.0f * M_PI), d = cod_rng_range(r, 25.0f, 75.0f);
    const u32 cp = cod_rng_pick(r, 4);
    cas_bark(a, cp == 0 ? CAS_BK_ADVANCE : (cp == 1 ? CAS_BK_FLANK : (cp == 2 ? CAS_BK_COPY : CAS_BK_SPOT)), true, (v3){lp.x + m_cosf(ang) * d, lp.y, lp.z + m_sinf(ang) * d}, 0.85f, (u32)cod_rng_int(r, 0, 9),
             false, false, 0.25f);
  }
}


void cod_audio_schedule(EcsWorld *w) {
  ecs_schedule_single(w, cod_audio_load_system);      // writes CodAudioState and CodLoad
  ecs_schedule_single(w, cod_audio_dev_system);       // writes the CodAudio dev switches
  ecs_schedule_single(w, cod_audio_begin_system);     // the CodAudio is one writer: the frame's flags and budgets
  ecs_schedule_single(w, cod_audio_listener_system);  // same writer: the listener
  ecs_schedule_single(w, cod_audio_space_system);     // same writer: the space probe and the convolver weights
  ecs_schedule_single(w, cod_audio_player_system);    // same writer: the player's and the explosions' events
  ecs_schedule_single(w, cod_audio_soldier_system);   // same writer: the soldiers' events
  ecs_schedule_single(w, cod_audio_ambience_system);  // same writer: the ambience timers
}
