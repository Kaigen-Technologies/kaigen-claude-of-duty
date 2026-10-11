// Claude of Duty HUD model: the systems that turn the packages' state and events into the HUD model.
#include "lib/random.h"

// the weapon labels the HUD reads
hz_internal const char *cod_hud_weapon_label(u32 id) {
  switch (id) {
  case COD_WEAPON_SMG: return "MPX-9";
  case COD_WEAPON_PISTOL: return "P-19";
  default: return "M4A1";
  }
}

hz_internal const char *cod_hud_fire_mode_label(u32 mode) {
  switch (mode) {
  case COD_FIRE_BURST: return "BURST";
  case COD_FIRE_SEMI: return "SEMI";
  default: return "AUTO";
  }
}

// copies ASCII upper-cased; returns the length
hz_internal u32 cod_hud_copy_upper(char *dst, u32 cap, const char *src, u32 src_len) {
  u32 n = src_len < cap - 1 ? src_len : cap - 1;
  for (u32 i = 0; i < n; i++) {
    char c = src[i];
    dst[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
  }
  dst[n] = 0;
  return n;
}

hz_internal String32Bytes cod_hud_str32(const char *src) {
  String32Bytes s = {0};
  s.len = cod_hud_copy_upper(s.value, sizeof(s.value), src, cstr_len(src));
  return s;
}

hz_internal String32Bytes cod_hud_str32_of(String32Bytes src) {
  String32Bytes s = {0};
  s.len = cod_hud_copy_upper(s.value, sizeof(s.value), src.value, src.len);
  return s;
}

// the banner channel restarts its enter on every message (2.1 s: 0.16 in, 0.462 out)
hz_internal void cod_hud_banner(EcsWorld *w, const char *title, const char *sub) {
  HudMsg msg = {.channel = HUD_MSG_BANNER, .duration = 2.1f - 0.16f - 0.462f};
  msg.title.len = cod_hud_copy_upper(msg.title.value, sizeof(msg.title.value), title, cstr_len(title));
  msg.body.len = cod_hud_copy_upper(msg.body.value, sizeof(msg.body.value), sub, cstr_len(sub));
  hud_msg_push(w, &msg);
}

hz_internal void cod_hud_push_kill(CodHudModel *m, const char *attacker, String32Bytes victim, b32 headshot, b32 mine,
                                   b32 attacker_enemy) {
  if (m->kill_count >= COD_HUD_EVENT_KILLS)
    return;
  CodHudKillRow *r = &m->kills[m->kill_count++];
  r->attacker = cod_hud_str32(attacker);
  r->victim = cod_hud_str32_of(victim);
  r->headshot = headshot;
  r->mine = mine;
  r->attacker_enemy = attacker_enemy;
}

hz_internal void cod_hud_push_hit(CodHudModel *m, u32 kind) {
  if (m->hit_count < COD_HUD_EVENT_HITS)
    m->hits[m->hit_count++] = (u8)kind;
}

hz_internal void cod_hud_push_hurt(CodHudModel *m, CodHudHurt h) {
  if (m->hurt_count < COD_HUD_EVENT_HURTS)
    m->hurts[m->hurt_count++] = h;
}

// a damage number: a popup entity the world_ui block projects; the HUD draws it with the motion
hz_internal void cod_hud_spawn_number(EcsWorld *w, CodHudModel *m, v3 at, f32 amount, u32 kind) {
  u32 slot = m->number_next++ % COD_HUD_NUMBERS;
  EcsEntity old = m->number_ring[slot];
  if (old && ecs_entity_is_alive(w, old))
    ecs_entity_destroy(w, old);
  Random rng = random_create((u64)m->frame * 7919u + (u64)slot * 104729u + 1u);
  f32 drift = random_f32_range(&rng, -1.0f, 1.0f) * 16.0f;
  f32 rise = 0.9f + random_f32(&rng) * 0.25f;
  WorldUiPopupDesc d = {.at = at, .life = kind == COD_NUM_KILL ? 1.25f : 0.95f};
  cstr_format(d.text.value, sizeof(d.text.value), "%", fmt_i32((i32)m_roundf(amount)));
  d.text.len = (u32)cstr_len(d.text.value);
  m->number_ring[slot] = world_ui_popup(w, &d);
  ecs_component_set(w, m->number_ring[slot], CodHudNumber, {.kind = kind, .drift = drift, .rise_scale = rise});
}

// the switches the HUD reads straight from the dev entity (the flow's CodDev does not carry them)
ecs_system(cod_hud_switches_system, EcsIn(CodDevSwitches), EcsResInOut(CodHudModel)) {
  CodHudModel *m = ecs_sys_get_resource_mut(CodHudModel);
  if (it->count <= 0)
    return;
  const CodDevSwitches *sw = ecs_sys_get_component(CodDevSwitches, 0);
  m->stats = sw->hud_stats;
  m->trace = sw->hud_trace;
}

// the model's frame: the clock, the live state, the player's and the weapons' events
ecs_system(cod_hud_model_system, EcsResInOut(CodHudModel), EcsResIn(CodClock), EcsResIn(CodDev), EcsResIn(CodPlayerState),
           EcsResIn(CodWeaponState)) {
  CodHudModel *m = ecs_sys_get_resource_mut(CodHudModel);
  const CodClock *clock = ecs_sys_get_resource(CodClock);
  const CodDev *dev = ecs_sys_get_resource(CodDev);
  const CodPlayerState *ps = ecs_sys_get_resource(CodPlayerState);
  const CodWeaponState *ws = ecs_sys_get_resource(CodWeaponState);

  // last frame's events and projections are gone
  m->fire_count = 0;
  m->flinch = 0.0f;
  m->hit_count = 0;
  m->hurt_count = 0;
  m->kill_count = 0;
  m->blip_count = 0;
  mem_set(m->pins, 0, sizeof(m->pins));
  mem_set(m->nades, 0, sizeof(m->nades));
  mem_set(m->numbers, 0, sizeof(m->numbers));
  mem_set(&m->prompt, 0, sizeof(m->prompt));

  m->frame = clock->frame;
  m->time = clock->time;
  m->dt = clock->live ? clock->dt : 0.0f;
  m->raw_dt = clock->raw_dt;
  m->hud_off = dev->hud_off;

  // ---- the camera after the frame's update ----
  if (ps->ready) {
    v3 fwd = quat_rotate(ps->view_rot, V3_BACK);
    v3 right = quat_rotate(ps->view_rot, V3_RIGHT);
    f32 rl = m_sqrtf(right.x * right.x + right.z * right.z);
    f32 fl = m_sqrtf(fwd.x * fwd.x + fwd.z * fwd.z);
    if (rl < 1e-6f)
      rl = 1.0f;
    if (fl < 1e-6f)
      fl = 1.0f;
    m->cam_pos = ps->eye;
    m->cam_fwd = fwd;
    m->cam_right = right;
    m->rx = right.x / rl;
    m->rz = right.z / rl;
    m->fx = fwd.x / fl;
    m->fz = fwd.z / fl;
    m->heading_deg = m_degrees(m_atan2f(m->fx, -m->fz));
    m->fov_deg = ps->fov;
    m->player_xz = (v2){ps->feet.x, ps->feet.z};
    m->camera_valid = true;
  }

  // ---- live state: the player and the weapon ----
  m->max_health = COD_PLAYER_MAX_HEALTH;
  m->max_armour = 150.0f;
  m->armour = 0.0f;
  m->time_left = 600.0f;
  if (ps->ready) {
    m->health = ps->health;
    m->regen = ps->regenerating;
    m->move = m_minf(1.0f, ps->speed / COD_PLAYER_TAC_SPRINT_SPEED);
    m->sprint = ps->sprinting || ps->tactical_sprint;
    m->crouch = ps->stance == COD_STANCE_CROUCH || ps->stance == COD_STANCE_PRONE;
    m->ads = ps->ads > 0.5f;
    m->airborne = !ps->grounded;
  }
  m->weapon = ws->weapon;
  m->fire_mode = ws->fire_mode;
  m->mag_size = ws->mag_size;
  // the count includes the chambered round, clamped to the magazine for the pips
  m->ammo = MIN(ws->mag + (ws->chambered ? 1u : 0u), ws->mag_size);
  m->reserve = ws->reserve;
  m->lethal = ws->grenades;
  m->tactical = ws->flashes;
  m->base_spread = 4.0f + m_saturatef(ws->spread_deg / 6.0f) * 40.0f;
  m->reloading = ws->reloading;
  m->reload_progress = ws->reloading ? ws->reload_progress : 0.0f;

  // ---- damage:taken: the player's applied hits ----
  for (u32 i = 0; i < ps->events.hit_count && i < COD_PLAYER_MAX_HITS; i++) {
    const CodPlayerHit *h = &ps->events.hits[i];
    f32 dx = 0.0f, dz = 1.0f;
    if (!h->directionless) {
      dx = h->from.x - ps->feet.x;
      dz = h->from.z - ps->feet.z;
    }
    cod_hud_push_hurt(m, (CodHudHurt){.dx = dx, .dz = dz, .damage = true, .amount = h->amount});
  }

  // ---- weapon:fire from the player's gun ----
  m->fire_count += ws->events.shot_count;

  if (m->trace) {
    log_info("hud t=% hp=% ammo=%/% weapon=% spread=% move=% heading=% fire=% hits=% hurts=% kills=%",
             fmt_f32_p(m->time, 3), fmt_f32_p(m->health, 1), fmt_u32(m->ammo), fmt_u32(m->reserve), fmt_u32(m->weapon),
             fmt_f32_p(m->base_spread, 2), fmt_f32_p(m->move, 3), fmt_f32_p(m->heading_deg, 2), fmt_u32(m->fire_count),
             fmt_u32(m->hit_count), fmt_u32(m->hurt_count), fmt_u32(m->kill_count));
  }
}

// the match score (the AI's tallies) and the reticle flinch of explosions near the player
ecs_system(cod_hud_world_system, EcsResInOut(CodHudModel), EcsResIn(CodAiState), EcsResIn(CodExplosions), EcsResIn(CodPlayerState)) {
  CodHudModel *m = ecs_sys_get_resource_mut(CodHudModel);
  const CodAiState *ai = ecs_sys_get_resource(CodAiState);
  const CodExplosions *ex = ecs_sys_get_resource(CodExplosions);
  const CodPlayerState *ps = ecs_sys_get_resource(CodPlayerState);
  m->score_us = ai->kills_by_player;
  m->score_them = ai->player_deaths;
  for (u32 i = 0; i < ex->count && i < COD_MAX_EXPLOSIONS; i++) {
    const CodExplosionEvent *e = &ex->items[i];
    f32 dx = e->position.x - ps->eye.x, dy = e->position.y - ps->eye.y, dz = e->position.z - ps->eye.z;
    f32 d = m_sqrtf(dx * dx + dy * dy + dz * dz);
    f32 radius = e->radius > 0.0f ? e->radius : 6.0f;
    if (d < radius * 2.5f)
      m->flinch = m_maxf(m->flinch, 0.6f);
  }
}

// the soldiers' events: the player's hits (hitmarkers, damage numbers, kills) and every death (killfeed)
ecs_system(cod_hud_soldiers_system, EcsIn(CodSoldier), EcsIn(CodSoldierEventsComp), EcsResInOut(CodHudModel),
           EcsResIn(CodWeaponState), EcsResIn(CodClock)) {
  CodHudModel *m = ecs_sys_get_resource_mut(CodHudModel);
  const CodWeaponState *ws = ecs_sys_get_resource(CodWeaponState);
  const CodClock *clock = ecs_sys_get_resource(CodClock);
  for (i32 i = 0; i < it->count; i++) {
    const CodSoldier *s = ecs_sys_get_component(CodSoldier, i);
    const CodSoldierEvents *ev = &ecs_sys_get_component(CodSoldierEventsComp, i)->ev;
    b32 fresh = ev->frame == clock->frame;
    // the weapon:fire handler kicks the reticle for every muzzle, enemy ones included
    if (fresh && ev->fired)
      m->fire_count++;

    u32 last = ~0u;
    for (u32 r = 0; r < ws->events.round_count && r < COD_WEAPON_MAX_ROUNDS; r++) {
      const CodRound *round = &ws->events.rounds[r];
      if (round->shooter.player && round->hit_soldier == it->entities[i] && round->hit_damage > 0.0f)
        last = r;
    }
    b32 killed_by_player = fresh && ev->killed && ev->killed_by_player;
    b32 kill_credited = false;
    for (u32 r = 0; r < ws->events.round_count && r < COD_WEAPON_MAX_ROUNDS; r++) {
      const CodRound *round = &ws->events.rounds[r];
      if (!round->shooter.player || round->hit_soldier != it->entities[i] || round->hit_damage <= 0.0f)
        continue;
      b32 killed = killed_by_player && r == last;
      b32 headshot = round->hit_zone == COD_ZONE_HEAD;
      b32 armour = fresh && ev->armor;
      u32 hit_kind = killed ? COD_HIT_KILL : headshot ? COD_HIT_HEAD : armour ? COD_HIT_ARMOUR : COD_HIT_HIT;
      u32 num_kind = killed ? COD_NUM_KILL : headshot ? COD_NUM_HS : armour ? COD_NUM_ARMOUR : COD_NUM_HIT;
      cod_hud_push_hit(m, hit_kind);
      cod_hud_spawn_number(it->world, m, round->hit_point, round->hit_damage, num_kind);
      if (killed) {
        kill_credited = true;
        cod_hud_push_kill(m, "YOU", s->name.len ? s->name : fixedstr32_from_cstr("ENEMY"), headshot, true, false);
        cod_hud_banner(it->world, "Enemy Eliminated", headshot ? "+150 XP \xC2\xB7 HEADSHOT" : "+100 XP");
      }
    }
    // a death nobody on the HUD's side was credited with (actor:death)
    if (fresh && ev->killed && !kill_credited)
      cod_hud_push_kill(m, "ENEMY", s->name.len ? s->name : fixedstr32_from_cstr("OPERATOR"), false, false, true);
  }
}

// objective pins: the projection's result, clamped to the margin ring
ecs_system(cod_hud_pins_system, EcsIn(WorldUi), EcsIn(WorldUiScreen), EcsIn(CodHudObjective), EcsResInOut(CodHudModel),
           EcsResIn(WorldUiView)) {
  CodHudModel *m = ecs_sys_get_resource_mut(CodHudModel);
  const WorldUiView *view = ecs_sys_get_resource(WorldUiView);
  f32 k = m_clampf(view->screen.y / 1080.0f, 0.62f, 2.4f);
  v2 centre = v2_scale(view->screen, 0.5f);
  f32 mx = centre.x - 74.0f * k, my = centre.y - 74.0f * k;
  u32 n = 0;
  for (i32 i = 0; i < it->count; i++) {
    while (n < COD_HUD_PINS && m->pins[n].active)
      n++;
    if (n >= COD_HUD_PINS)
      break;
    const WorldUi *u = ecs_sys_get_component(WorldUi, i);
    const WorldUiScreen *s = ecs_sys_get_component(WorldUiScreen, i);
    const CodHudObjective *o = ecs_sys_get_component(CodHudObjective, i);
    f32 dx = s->pos.x - centre.x, dy = s->pos.y - centre.y;
    b32 off = !s->in_front;
    if (m_absf(dx) > mx || m_absf(dy) > my) {
      off = true;
      f32 sc = m_minf(mx / (m_absf(dx) > 1e-4f ? m_absf(dx) : 1e-4f), my / (m_absf(dy) > 1e-4f ? m_absf(dy) : 1e-4f));
      dx *= sc;
      dy *= sc;
    }
    m->pins[n] = (CodHudPinView){.active = true,
                                 .label = o->label,
                                 .name = o->name,
                                 .world_xz = {u->offset.x, u->offset.z},
                                 .screen = {centre.x + dx, centre.y + dy},
                                 .distance = s->distance,
                                 .offscreen = off,
                                 .angle_deg = m_degrees(m_atan2f(dy, dx)) + 90.0f};
  }
}

// grenade danger markers: the same ring with a 56 px margin
ecs_system(cod_hud_nades_system, EcsIn(WorldUiScreen), EcsIn(WorldUiPopup), EcsIn(CodHudGrenade), EcsResInOut(CodHudModel),
           EcsResIn(WorldUiView)) {
  CodHudModel *m = ecs_sys_get_resource_mut(CodHudModel);
  const WorldUiView *view = ecs_sys_get_resource(WorldUiView);
  f32 k = m_clampf(view->screen.y / 1080.0f, 0.62f, 2.4f);
  v2 centre = v2_scale(view->screen, 0.5f);
  f32 mx = centre.x - 56.0f * k, my = centre.y - 56.0f * k;
  u32 n = 0;
  for (i32 i = 0; i < it->count; i++) {
    while (n < COD_HUD_NADES && m->nades[n].active)
      n++;
    if (n >= COD_HUD_NADES)
      break;
    const WorldUiScreen *s = ecs_sys_get_component(WorldUiScreen, i);
    const WorldUiPopup *p = ecs_sys_get_component(WorldUiPopup, i);
    f32 dx = s->pos.x - centre.x, dy = s->pos.y - centre.y;
    if (m_absf(dx) > mx || m_absf(dy) > my || !s->in_front) {
      f32 sc = m_minf(mx / (m_absf(dx) > 1e-4f ? m_absf(dx) : 1e-4f), my / (m_absf(dy) > 1e-4f ? m_absf(dy) : 1e-4f));
      dx *= sc;
      dy *= sc;
    }
    m->nades[n] = (CodHudNadeView){.active = true, .screen = {centre.x + dx, centre.y + dy}, .distance = s->distance, .age = p->age, .life = p->life};
  }
}

// damage numbers: the projection's result and the popup's age
ecs_system(cod_hud_numbers_system, EcsIn(WorldUiLabel), EcsIn(WorldUiScreen), EcsIn(WorldUiPopup), EcsIn(CodHudNumber),
           EcsResInOut(CodHudModel)) {
  CodHudModel *m = ecs_sys_get_resource_mut(CodHudModel);
  u32 n = 0;
  for (i32 i = 0; i < it->count; i++) {
    while (n < COD_HUD_NUMBERS && m->numbers[n].active)
      n++;
    if (n >= COD_HUD_NUMBERS)
      break;
    const WorldUiLabel *l = ecs_sys_get_component(WorldUiLabel, i);
    const WorldUiScreen *s = ecs_sys_get_component(WorldUiScreen, i);
    const WorldUiPopup *p = ecs_sys_get_component(WorldUiPopup, i);
    const CodHudNumber *c = ecs_sys_get_component(CodHudNumber, i);
    CodHudNumberView *v = &m->numbers[n];
    v->active = true;
    v->text.len = l->text.len < sizeof(v->text.value) - 1 ? l->text.len : (u32)sizeof(v->text.value) - 1;
    mem_cpy(v->text.value, l->text.value, v->text.len);
    v->text.value[v->text.len] = 0;
    v->kind = c->kind;
    v->screen = s->pos;
    v->distance = s->distance;
    v->in_front = s->in_front;
    v->age = p->age;
    v->life = p->life;
    v->drift = c->drift;
    v->rise_scale = c->rise_scale;
  }
}

// the prompt: the nearest active one
ecs_system(cod_hud_prompt_system, EcsIn(WorldUiLabel), EcsIn(WorldUiScreen), EcsIn(WorldUiPrompt), EcsIn(CodHudPromptBar),
           EcsResInOut(CodHudModel)) {
  CodHudModel *m = ecs_sys_get_resource_mut(CodHudModel);
  for (i32 i = 0; i < it->count; i++) {
    const WorldUiScreen *s = ecs_sys_get_component(WorldUiScreen, i);
    if (!s->active)
      continue;
    const WorldUiLabel *l = ecs_sys_get_component(WorldUiLabel, i);
    const WorldUiPrompt *p = ecs_sys_get_component(WorldUiPrompt, i);
    const CodHudPromptBar *b = ecs_sys_get_component(CodHudPromptBar, i);
    CodHudPromptView *v = &m->prompt;
    v->active = true;
    v->key = p->key < KEY_MAX ? fixedstr32_from_cstr((char *)input_button_names[p->key]) : (String32Bytes){0};
    v->text.len = l->text.len < sizeof(v->text.value) - 1 ? l->text.len : (u32)sizeof(v->text.value) - 1;
    mem_cpy(v->text.value, l->text.value, v->text.len);
    v->text.value[v->text.len] = 0;
    v->sub.len = l->sub.len < sizeof(v->sub.value) - 1 ? l->sub.len : (u32)sizeof(v->sub.value) - 1;
    mem_cpy(v->sub.value, l->sub.value, v->sub.len);
    v->sub.value[v->sub.len] = 0;
    v->has_bar = b->has_bar;
    v->progress = b->progress;
    break;
  }
}
