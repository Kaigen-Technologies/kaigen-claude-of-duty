// Claude of Duty audio, voices: spatialisation, the clip choice, the commands the main lane starts, and the per-frame mixer drive.

/* ------------------------------------------------------------ spatial maths */

// close to physical below 40 m, gentler beyond (gunfire at 150 m is still clearly audible)
hz_internal f32 cas_attenuation(f32 dist) {
  const f32 ref = 2.0f;
  const f32 near_g = ref / (ref + 0.85f * m_maxf(0.0f, dist - ref));
  const f32 far_g = 0.055f * m_powf(60.0f / m_maxf(dist, 60.0f), 0.55f);
  return cas_clamp(m_maxf(near_g, dist > 45.0f ? far_g : 0.0f), 0.0f, 1.0f);
}

// equal-power pan from the listener-relative azimuth, folded to the front
hz_internal f32 cas_pan(const CodAudio *a, v3 position) {
  const v3 rel = v3_sub(position, a->listener_pos);
  const f32 len = v3_length(rel);
  if (len <= 1e-6f)
    return 0.0f;
  const v3 dir = v3_scale(rel, 1.0f / len);
  const v3 right = v3_normalize(v3_cross(a->listener_fwd, a->listener_up));
  const v3 up = v3_cross(right, v3_normalize(a->listener_fwd));
  const v3 flat = v3_sub(dir, v3_scale(up, v3_dot(dir, up)));
  const f32 flat_len = v3_length(flat);
  if (flat_len <= 1e-6f)
    return 0.0f;
  const f32 d = m_clampf(v3_dot(v3_scale(flat, 1.0f / flat_len), right), -1.0f, 1.0f);
  return 1.0f - 2.0f * m_acosf(d) / M_PI;
}

// the first static-world hit along a segment
hz_internal b32 cas_ray_world(const CodAudio *a, v3 from, v3 to, f32 *dist) {
  const EcsCollisionLayer *layer = ecs_physics_get_static_layer(a->world);
  if (!layer)
    return false;
  PhysicsRaycastResult r = {0};
  i32 idx = 0;
  if (!physics_raycast_layer(from, to, (CollisionLayer *)&layer->layer, PHYSICS_QUERY_FILTER_ALL, &r, &idx))
    return false;
  *dist = r.fraction * v3_distance(from, to);
  return true;
}

// two rays (ear height and raised) against the world, 0 clear .. 1 thick wall
hz_internal f32 cas_occlusion(const CodAudio *a, v3 p) {
  if (v3_distance(p, a->listener_pos) < 0.8f)
    return 0.0f;
  f32 blocked = 0.0f;
  for (u32 i = 0; i < 2; i++) {
    const f32 lift = i == 0 ? 0.0f : 0.55f;
    const v3 o = {a->listener_pos.x, a->listener_pos.y + lift, a->listener_pos.z};
    const v3 target = {p.x, p.y + lift * 0.5f, p.z};
    const v3 dir = v3_sub(target, o);
    const f32 len = v3_length(dir);
    if (len < 1e-4f)
      continue;
    const v3 end = v3_add(o, v3_scale(dir, (len - 0.25f) / len));
    f32 hit = 0.0f;
    if (cas_ray_world(a, o, end, &hit))
      blocked += hit < len * 0.9f ? 1.0f : 0.5f;
  }
  return cas_clamp(blocked * 0.5f, 0.0f, 1.0f);
}

/* ------------------------------------------------------------ clip choice */

// the filtered clip level a cutoff needs: 0 clear, 1..3 the filtered copies
hz_internal u32 cas_filter_level(f32 cut_hz) { return cut_hz >= 11000.0f ? 0u : (cut_hz >= 4500.0f ? 1u : (cut_hz >= 1400.0f ? 2u : 3u)); }

// a random variant that is not the one played last
hz_internal u32 cas_pick_variant(CodAudio *a, CasSet *set) {
  u32 v = cod_rng_u32(&a->rng) % set->variants;
  if (set->variants > 1 && v == set->last)
    v = (v + 1u) % set->variants;
  set->last = v;
  return v;
}

// the clip of a set for a voice that needs `cut_hz` of lowpass; sets without filtered copies play clear
hz_internal u32 cas_pick_clip(CodAudio *a, u32 set_index, f32 cut_hz) {
  CasSet *set = &a->sets[set_index];
  const u32 f = cas_filter_level(cut_hz);
  if (f && set->filtered)
    return set->first + set->variants + f - 1u;
  return set->first + cas_pick_variant(a, set);
}

// the shot set nearest a distance among those rendered for the profile (bucket 0 is the dry, first-person set)
hz_internal u32 cas_shot_set(const CodAudio *a, u32 profile, f32 dist, b32 fp) {
  const u32 base = a->base[CAS_K_SHOT] + profile * (CAS_SHOT_BUCKETS + 1u);
  if (fp)
    return base;
  u32 best = base + 1u;
  f32 best_d = 1e9f;
  for (u32 b = 0; b < CAS_SHOT_BUCKETS; b++) {
    if (!a->sets[base + 1u + b].variants)
      continue;
    const f32 d = m_absf(m_logf(m_maxf(dist, 1.0f) / CAS_SHOT_DIST[b]));
    if (d < best_d) {
      best_d = d;
      best = base + 1u + b;
    }
  }
  return best;
}

hz_internal u32 cas_explosion_set(const CodAudio *a, f32 radius, f32 dist) {
  const u32 z = radius < 8.75f ? 0u : (radius < 13.5f ? 1u : 2u);
  const u32 base = a->base[CAS_K_EXPLOSION] + z * CAS_EXP_BUCKETS;
  u32 best = base;
  f32 best_d = 1e9f;
  for (u32 b = 0; b < CAS_EXP_BUCKETS; b++) {
    if (!a->sets[base + b].variants)
      continue;
    const f32 d = m_absf(m_logf(m_maxf(dist, 1.0f) / CAS_EXP_DIST[b]));
    if (d < best_d) {
      best_d = d;
      best = base + b;
    }
  }
  return best;
}

/* ------------------------------------------------------------ commands */

hz_internal void cas_emit(CodAudio *a, CasCmd cmd) {
  if (cmd.volume <= 1e-6f)
    return;
  if (a->cmds.len >= CAS_MAX_CMDS) {
    a->dropped++;
    return;
  }
  a->cmds.items[a->cmds.len++] = cmd;
}

typedef struct CasPlay {
  u32 set;
  v3 pos;
  f32 gain;        // opts.gain; 0 = 1
  f32 level;       // the event's scale on the voice (energy, level); 0 = 1
  f32 send;        // the voice's send; < 0 = the set's
  f32 pitch;       // 0 = 1
  f32 pitch_jitter;
  f32 extra_delay;
  b32 no_delay;
  f32 occlusion;   // < 0 = measured
  f32 max_dist;    // 0 = 320
  b32 baked_air;   // the clip already carries the air absorption of its distance
  u32 bus;         // 0 = the set's
} CasPlay;

hz_internal f32 cas_pitch(CodAudio *a, f32 base, f32 jitter) {
  const f32 p = base > 0.0f ? base : 1.0f;
  return jitter > 0.0f ? p * (1.0f + (cod_rng_float(&a->rng) * 2.0f - 1.0f) * jitter) : p;
}

// propagation delay, distance, occlusion, air absorption and the reverb send, panned by azimuth
hz_internal b32 cas_play_at(CodAudio *a, const CasPlay *p) {
  const CasSet *set = &a->sets[p->set];
  if (!set->variants)
    return false;
  const f32 d = v3_distance(p->pos, a->listener_pos);
  if (d > (p->max_dist > 0.0f ? p->max_dist : 320.0f))
    return false;
  const f32 occ = p->occlusion >= 0.0f ? p->occlusion : cas_occlusion(a, p->pos);
  const f32 atten = cas_attenuation(d) * (1.0f - 0.62f * occ);
  const f32 gain = cas_clamp(atten * (p->gain > 0.0f ? p->gain : 1.0f), 0.0f, 4.0f);
  const f32 occ_cut = cas_clamp(20000.0f * m_powf(0.021f, occ), 300.0f, 20000.0f);
  const f32 cut = m_minf(occ_cut, p->baked_air ? 20000.0f : cas_air_cutoff(d));
  const u32 clip = cas_pick_clip(a, p->set, cut);
  const f32 send_ref = p->send >= 0.0f ? p->send : set->send;
  const f32 send = cas_clamp(send_ref * (0.5f + m_minf(d, 90.0f) * 0.022f) * (1.0f + occ * 0.7f), 0.0f, 3.0f);
  const f32 level = p->level > 0.0f ? p->level : 1.0f;
  cas_emit(a, (CasCmd){.clip = clip,
                       .volume = gain * level * a->clips[clip].level * CAS_SQRT2,
                       .pitch = cas_pitch(a, p->pitch, p->pitch_jitter),
                       .bus = p->bus ? p->bus : set->bus,
                       .send = send / CAS_SEND_SCALE,
                       .positional = true,
                       .pos = p->pos,
                       .pan = cas_pan(a, p->pos),
                       .delay = (p->no_delay ? 0.0f : d / CAS_SPEED_OF_SOUND) + p->extra_delay,
                       .ambience = set->kind == CAS_K_AMBIENT});
  return true;
}

// head-locked, no distance or delay; the send is the call's level times the voice's
hz_internal void cas_play_dry(CodAudio *a, u32 set_index, f32 gain, f32 send_level, f32 pitch, u32 bus) {
  const CasSet *set = &a->sets[set_index];
  if (!set->variants)
    return;
  const u32 clip = cas_pick_clip(a, set_index, 20000.0f);
  const f32 send = cas_clamp(send_level * set->send, 0.0f, 3.0f);
  cas_emit(a, (CasCmd){.clip = clip,
                       .volume = (gain > 0.0f ? gain : 1.0f) * a->clips[clip].level * CAS_SQRT2,
                       .pitch = pitch > 0.0f ? pitch : 1.0f,
                       .bus = bus ? bus : set->bus,
                       .send = send / CAS_SEND_SCALE});
}

/* ------------------------------------------------------------ mixer controls */

// the ambience, foley and voice buses
hz_internal void cas_duck(CodAudio *a, f32 amount, f32 hold) {
  cas_duck_hit(&a->duck[CAS_DUCK_AMBIENCE], &a->amb_duck_target, &a->amb_duck_tau, CAS_DUCK_AMBIENCE, amount, hold);
  cas_duck_hit(&a->duck[CAS_DUCK_FOLEY], &a->bus[CAS_BUS_FOLEY].duck_target, &a->bus[CAS_BUS_FOLEY].duck_tau, CAS_DUCK_FOLEY, amount, hold);
  cas_duck_hit(&a->duck[CAS_DUCK_VOICE], &a->bus[CAS_BUS_VOICE].duck_target, &a->bus[CAS_BUS_VOICE].duck_tau, CAS_DUCK_VOICE, amount, hold);
}

// muffles the world and starts the tinnitus that outlives it
hz_internal void cas_concuss(CodAudio *a, f32 level) {
  level = cas_clamp(level, 0.0f, 1.0f);
  if (level <= a->deafness)
    return;
  a->deafness = level;
  a->muffle_lp_target = cas_clamp(20000.0f * m_powf(0.024f, level), 320.0f, 20000.0f);
  a->muffle_hs_target = -22.0f * level;
  a->muffle_gain_target = 1.0f - 0.55f * level;
  a->muffle_tau = 0.02f;
  a->master.tin_target = 0.05f * level;
  a->master.tin_tau = 0.03f;
  a->tin_until = (f32)a->clock + 4.0f + 7.0f * level;
}

hz_internal f32 cas_ease(f32 v, f32 target, f32 tau, f32 dt) { return v + (target - v) * (1.0f - m_expf(-dt / m_maxf(tau, 1e-4f))); }

// duck recovery, the deafness recovery and the tinnitus tail
hz_internal void cas_mixer_update(CodAudio *a, f32 dt) {
  for (u32 b = 0; b < CAS_DUCK_COUNT; b++) {
    f32 *target = b == CAS_DUCK_AMBIENCE ? &a->amb_duck_target : (b == CAS_DUCK_FOLEY ? &a->bus[CAS_BUS_FOLEY].duck_target : &a->bus[CAS_BUS_VOICE].duck_target);
    f32 *tau = b == CAS_DUCK_AMBIENCE ? &a->amb_duck_tau : (b == CAS_DUCK_FOLEY ? &a->bus[CAS_BUS_FOLEY].duck_tau : &a->bus[CAS_BUS_VOICE].duck_tau);
    cas_duck_update(&a->duck[b], target, tau, dt);
  }
  a->amb_duck = cas_ease(a->amb_duck, a->amb_duck_target, a->amb_duck_tau, dt);
  if (a->deafness > 0.0f) {
    a->deafness = m_maxf(0.0f, a->deafness - dt * (0.1f + a->deafness * 0.22f));
    a->muffle_lp_target = cas_clamp(20000.0f * m_powf(0.024f, a->deafness), 320.0f, 20000.0f);
    a->muffle_hs_target = -22.0f * a->deafness;
    a->muffle_gain_target = 1.0f - 0.55f * a->deafness;
    a->muffle_tau = 0.25f;
  }
  a->muffle.lp_hz = cas_ease(a->muffle.lp_hz, a->muffle_lp_target, a->muffle_tau, dt);
  a->muffle.hs_db = cas_ease(a->muffle.hs_db, a->muffle_hs_target, a->muffle_tau, dt);
  a->muffle.gain = cas_ease(a->muffle.gain, a->muffle_gain_target, a->muffle_tau, dt);
  if (a->master.tin_target > 0.0f && (f32)a->clock > a->tin_until - 3.5f) {
    a->master.tin_target = 0.0f;
    a->master.tin_tau = 1.1f;
  }
}

/* ------------------------------------------------------------ the main lane's frame */

hz_internal void cas_start_cmd(CodAudio *a, const CasCmd *c) {
  const CasClip *clip = &a->clips[c->clip];
  if (!handle_is_valid(clip->src))
    return;
  AudioPlayDesc d = {.volume = c->volume, .pitch = c->pitch > 0.0f ? c->pitch : 1.0f, .bus = (AudioBus)c->bus, .send = c->send, .pan = c->pan};
  HzHandleT(AudioVoice) voice = audio_play(&a->system, clip->src, &d);
  if (!handle_is_valid(voice))
    return;
  a->plays++;
  if (a->tracks.len < CAS_MAX_TRACKS)
    a->tracks.items[a->tracks.len++] = (CasTrack){.voice = voice, .pos = c->pos, .positional = c->positional, .ambience = c->ambience,
                                                  .base_volume = c->volume};
}

// finished voices leave the list; positional ones follow the listener, ambience ones the duck
hz_internal void cas_tracks_update(CodAudio *a) {
  for (u32 i = 0; i < a->tracks.len;) {
    CasTrack *t = &a->tracks.items[i];
    if (!audio_voice_playing(&a->system, t->voice)) {
      *t = a->tracks.items[--a->tracks.len];
      continue;
    }
    if (t->positional)
      audio_voice_set_pan(&a->system, t->voice, cas_pan(a, t->pos));
    if (t->ambience)
      audio_voice_set_volume(&a->system, t->voice, m_maxf(t->base_volume * a->amb_duck, 1e-5f));
    i++;
  }
}

// the bed streams ahead of the mixer: whatever the ring is short of is synthesised now
hz_internal void cas_bed_feed(CodAudio *a, f32 dt) {
  (void)dt;
  if (!handle_is_valid(a->bed_src))
    return;
  const u32 want = (u32)(CAS_BED_AHEAD_S * (f32)a->sr);
  const u32 have = audio_source_stream_buffered_frames(&a->system, a->bed_src);
  if (have >= want)
    return;
  u32 need = want - have;
  TempAllocator ta = tctx_temp_allocator_begin(NULL);
  const u32 chunk = 2048u;
  f32 *lr = ALLOC_ARRAY(&ta.allocator, f32, chunk * 2u);
  i16 *pcm = ALLOC_ARRAY(&ta.allocator, i16, chunk * 2u);
  while (need > 0) {
    const u32 n = MIN(need, chunk);
    cas_bed_render(&a->bed, lr, n, (f32)n / (f32)a->sr);
    pg_buf_to_pcm16(lr, n * 2u, pcm);
    audio_source_stream_write(&a->system, a->bed_src, (const u8 *)pcm, n * 2u * (u32)sizeof(i16));
    need -= n;
  }
  tctx_temp_allocator_end(ta);
}

hz_internal void cas_bed_start(CodAudio *a) {
  a->bed_src = audio_source_stream(&a->system, &(AudioStreamSourceDesc){.sample_rate = a->sr, .channels = 2, .mode = AUDIO_STREAM_BUFFERED,
                                                                         .buffer_capacity_bytes = a->sr * 2u * (u32)sizeof(i16) / 2u, .alloc = &a->alloc});
  cas_bed_feed(a, 0.0f);
  a->bed_voice = audio_play(&a->system, a->bed_src,
                            &(AudioPlayDesc){.volume = CAS_BED_VOLUME, .bus = CAS_BUS_WEAPONS, .send = 0.22f / CAS_SEND_SCALE});
}

