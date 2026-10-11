// Claude of Duty AI consolidation: the frame's shared state, the roster, squads, cover claims and the events soldiers react to.

// the clock, the player and the sky for every soldier this frame; resets the lists the later systems fill
ecs_system(cai_frame_system, EcsResInOut(CodAiShared), EcsResIn(CodClock), EcsResIn(CodPlayerState), EcsResIn(CodLook)) {
  CodAiShared *sh = ecs_sys_get_resource_mut(CodAiShared);
  const CodClock *clock = ecs_sys_get_resource(CodClock);
  const CodPlayerState *ps = ecs_sys_get_resource(CodPlayerState);
  const CodLook *look = ecs_sys_get_resource(CodLook);
  sh->live = clock->live;
  sh->dt = clock->live ? clock->dt : 0.0f;
  sh->time = clock->time;
  sh->frame = clock->frame;
  sh->player_ready = ps->ready;
  sh->player_chest = v3_add(ps->feet, (v3){0.0f, 1.35f, 0.0f});
  sh->footstep = ps->events.footstep;
  sh->footstep_pos = ps->events.footstep_position;
  sh->footstep_running = ps->events.footstep_running;
  sh->sun = look->sun_dir;
  if (v3_lengthsq(sh->sun) < 1e-8f)
    sh->sun = (v3){0.0f, 1.0f, 0.0f};
  sh->frustum_ok = false;
  sh->roster_count = 0;
  mem_zero(sh->roster, sizeof(sh->roster));
}

// the camera's frustum for the relevance test
ecs_system(cai_camera_system, EcsIn(Camera), EcsResInOut(CodAiShared)) {
  CodAiShared *sh = ecs_sys_get_resource_mut(CodAiShared);
  if (it->count <= 0)
    return;
  const Camera *cam = ecs_sys_get_component(Camera, 0);
  for (u32 i = 0; i < 6; i++)
    sh->planes[i] = cam->frustum.planes[i];
  sh->frustum_ok = true;
}

// the AI dev switches
ecs_system(cai_switch_system, EcsIn(CodDevSwitches), EcsResInOut(CodAiShared), EcsResInOut(EcsNavigationDebug)) {
  CodAiShared *sh = ecs_sys_get_resource_mut(CodAiShared);
  if (it->count <= 0)
    return;
  const CodDevSwitches *sw = ecs_sys_get_component(CodDevSwitches, 0);
  sh->trace = sw->ai_trace;
  sh->debug = sw->ai_debug;
  // level 3 draws the soldiers' navmesh around the player
  EcsNavigationDebug *nd = ecs_sys_get_resource_mut(EcsNavigationDebug);
  b32 draw = sw->ai_debug >= 3;
  if (nd->draw_navmesh != draw || (draw && v3_distance(nd->focus, sh->player_chest) > 2.0f)) {
    nd->draw_navmesh = draw;
    nd->focus = sh->player_chest;
  }
}

// every soldier's public face for the others: who, where, what it knows and what it asks of its squad
ecs_system(cai_roster_system, EcsIn(CodSoldierBrain), EcsResInOut(CodAiShared)) {
  CodAiShared *sh = ecs_sys_get_resource_mut(CodAiShared);
  for (i32 i = 0; i < it->count; i++) {
    const CodSoldierBrain *b = ecs_sys_get_component(CodSoldierBrain, i);
    assert_msg(b->slot < CAI_MAX_SOLDIERS, "cai: roster slot");
    CaiRoster *r = &sh->roster[b->slot];
    *r = (CaiRoster){.entity = it->entities[i],
                     .used = true,
                     .alive = b->alive,
                     .id = b->id,
                     .squad = b->squad,
                     .mode = b->mode,
                     .position = b->position,
                     .radius = b->radius,
                     .has_target = b->has_target,
                     .target_visible = b->target_visible,
                     .last_known = b->last_known,
                     .peek_ask = b->peek_ask,
                     .flank_ask = b->flank_ask,
                     .grenade_ask = b->grenade_ask,
                     .cover = b->alive ? b->cover : -1,
                     .nav_req = b->nav_req};
    if (b->slot + 1 > sh->roster_count)
      sh->roster_count = b->slot + 1;
  }
}

hz_internal void cai_add_noise(CodAiShared *sh, v3 p) {
  if (sh->noise_count < CAI_MAX_NOISE)
    sh->noise[sh->noise_count++] = p;
}

// one traced round's impacts: noise for everyone, and a hit for every hit volume it entered
hz_internal void cai_gather_round(EcsWorld *w, CodAiShared *sh, const CodRound *round, b32 from_player) {
  for (u32 k = 0; k < round->impact_count; k++) {
    const CodImpact *im = &round->impacts[k];
    cai_add_noise(sh, im->point);
    if (im->exit || !im->entity)
      continue;
    const CodHitbox *hb = ecs_component_get(w, im->entity, CodHitbox);
    if (!hb || hb->zone >= COD_ZONE_COUNT || sh->hit_count >= CAI_MAX_HITS)
      continue;
    if (!from_player && hb->soldier == round->shooter.soldier)
      continue;
    sh->hits[sh->hit_count++] = (CaiHit){.soldier = hb->soldier,
                                         .zone = hb->zone,
                                         .damage = im->damage * cai_zone_scale[hb->zone],
                                         .point = im->point,
                                         .dir = im->incident,
                                         .from_player = from_player};
  }
}

// contact sharing, the flanker, the peek tokens, and the grants for what members asked for last frame
hz_internal void cai_squads_update(CodAiShared *sh) {
  f32 dt = sh->dt;
  for (u32 s = 0; s < CAI_MAX_SQUADS; s++) {
    CaiSquad *sq = &sh->squads[s];
    if (!sq->used)
      continue;
    u32 sid = s + 1;
    sq->grenade_cooldown -= dt;
    sq->contact_age += dt;

    sq->combat_count = 0;
    for (u32 i = 0; i < sh->roster_count; i++) {
      const CaiRoster *m = &sh->roster[i];
      if (m->used && m->squad == sid && m->alive && (m->mode == COD_AI_COMBAT || m->mode == COD_AI_SUPPRESSED))
        sq->combat_count++;
    }

    if (sq->flanker) {
      b32 keep = false;
      for (u32 i = 0; i < sh->roster_count; i++) {
        const CaiRoster *m = &sh->roster[i];
        if (m->used && m->squad == sid && m->id == sq->flanker && m->alive && (m->mode == COD_AI_FLANK || m->flank_ask))
          keep = true;
      }
      if (!keep)
        sq->flanker = 0;
    }

    // whoever can see the player broadcasts; the members apply it with a call-out delay
    for (u32 i = 0; i < sh->roster_count; i++) {
      const CaiRoster *m = &sh->roster[i];
      if (m->used && m->squad == sid && m->alive && m->has_target && m->target_visible) {
        sq->contact = m->last_known;
        sq->has_contact = true;
        sq->contact_age = 0.0f;
        break;
      }
    }

    // rotate the peek tokens so the same man is not always exposed
    sq->peek_timer -= dt;
    if (sq->peek_timer <= 0.0f) {
      sq->peek_timer = 1.1f + cod_rng_float(&sq->rng) * 1.2f;
      sq->holder_count = 0;
    }
    // askers who can see the player first, round-robin from the slot after the last grant so no member starves
    for (u32 pass = 0; pass < 2; pass++) {
      for (u32 k = 0; k < sh->roster_count; k++) {
        u32 i = (sq->peek_cursor + k) % sh->roster_count;
        const CaiRoster *m = &sh->roster[i];
        if (!m->used || m->squad != sid || !m->alive || !m->peek_ask || (pass == 0 && !m->target_visible))
          continue;
        b32 holds = false;
        for (u32 h = 0; h < sq->holder_count; h++)
          if (sq->holders[h] == m->id)
            holds = true;
        if (!holds && sq->holder_count < sq->peek_tokens && sq->holder_count < CAI_MAX_SOLDIERS) {
          sq->holders[sq->holder_count++] = m->id;
          sq->peek_cursor = i + 1;
        }
      }
    }

    // grenades are rationed and one man at a time flanks
    sq->grenade_grant = 0;
    if (sq->grenade_cooldown <= 0.0f) {
      for (u32 i = 0; i < sh->roster_count; i++) {
        const CaiRoster *m = &sh->roster[i];
        if (m->used && m->squad == sid && m->alive && m->grenade_ask) {
          sq->grenade_grant = m->id;
          sq->grenade_cooldown = 14.0f + cod_rng_float(&sq->rng) * 12.0f;
          break;
        }
      }
    }
    if (sq->flanker == 0) {
      for (u32 i = 0; i < sh->roster_count; i++) {
        const CaiRoster *m = &sh->roster[i];
        if (m->used && m->squad == sid && m->alive && m->flank_ask && sq->combat_count >= 2) {
          sq->flanker = m->id;
          break;
        }
      }
    }
  }
}

// cover points held: the first claimant of a point keeps it, a second is told to pick again
hz_internal void cai_claim_cover(CodAiShared *sh) {
  CaiNav *nav = sh->nav;
  if (!nav || !nav->ready)
    return;
  mem_zero(nav->claim, nav->cover_count * sizeof(u32));
  for (u32 i = 0; i < sh->roster_count; i++) {
    CaiRoster *r = &sh->roster[i];
    if (!r->used || !r->alive || r->cover < 0 || (u32)r->cover >= nav->cover_count)
      continue;
    if (nav->claim[r->cover] == 0 || nav->claim[r->cover] == r->id)
      nav->claim[r->cover] = r->id;
    else
      r->cover_lost = true;
  }
}

// this frame's shots, impacts, hits and blasts, flattened for the per-soldier systems; then the squads and cover claims
ecs_system(cai_events_system, EcsResInOut(CodAiShared), EcsResIn(CodWeaponState), EcsResIn(CodExplosions)) {
  CodAiShared *sh = ecs_sys_get_resource_mut(CodAiShared);
  const CodWeaponState *ws = ecs_sys_get_resource(CodWeaponState);
  const CodExplosions *ex = ecs_sys_get_resource(CodExplosions);
  EcsWorld *w = it->world;
  sh->hit_count = sh->shot_count = sh->noise_count = 0;

  // the player's shots are heard by everybody; ours are not (weapon:fire from 'ai_rifle' is ignored)
  for (u32 i = 0; i < ws->events.shot_count && i < COD_WEAPON_MAX_SHOTS; i++) {
    const CodShotEvent *s = &ws->events.shots[i];
    if (s->shooter.player && sh->shot_count < COD_WEAPON_MAX_SHOTS)
      sh->shots[sh->shot_count++] = (CaiShot){s->origin, s->dir};
  }
  for (u32 i = 0; i < ws->events.round_count && i < COD_WEAPON_MAX_ROUNDS; i++)
    cai_gather_round(w, sh, &ws->events.rounds[i], true);

  // the soldiers' rounds fired last frame reach the others now
  for (u32 i = 0; i < sh->roster_count; i++) {
    const CaiRoster *r = &sh->roster[i];
    if (!r->used)
      continue;
    const CodSoldierEventsComp *evc = ecs_component_get(w, r->entity, CodSoldierEventsComp);
    if (evc && evc->ev.fired && evc->ev.frame + 1 == sh->frame)
      cai_gather_round(w, sh, &evc->ev.round, false);
  }

  sh->explosion_count = ex->count < COD_MAX_EXPLOSIONS ? ex->count : COD_MAX_EXPLOSIONS;
  for (u32 i = 0; i < sh->explosion_count; i++)
    sh->explosions[i] = ex->items[i];

  cai_squads_update(sh);
  cai_claim_cover(sh);
}
