// Claude of Duty weapons: the weapon data, the deterministic recoil patterns and the keyframed reload, inspect, draw and holster clips.

hz_internal const CwpDef cwp_defs[COD_WEAPON_COUNT] = {
    [COD_WEAPON_RIFLE] =
        {
            .label = "M4A1",
            .mode_count = 3,
            .modes = {COD_FIRE_AUTO, COD_FIRE_BURST, COD_FIRE_SEMI},
            .rpm = 800.0f,
            .burst_count = 3,
            .burst_rpm = 950.0f,
            .burst_delay = 0.16f,
            .mag_size = 30,
            .reserve = 210,
            .muzzle_velocity = 880.0f,
            .damage = 33.0f,
            .penetration = 1.0f,
            .dropoff = 0.62f,
            .max_range = 420.0f,
            .drag_k = 0.28f,
            .tracer_every = 3,
            .spread_hip = 2.05f,
            .spread_ads = 0.24f,
            .spread_per_shot = 0.3f,
            .spread_max = 3.4f,
            .spread_decay = 3.6f,
            .recoil = {.pitch = 0.0085f,
                       .yaw = 0.0022f,
                       .kick_back = 0.019f,
                       .kick_up = 0.0072f,
                       .roll = 0.032f,
                       .punch = 0.35f,
                       .freq = 8.5f,
                       .damping = 0.42f,
                       .pattern_length = 30,
                       .pattern_seed = 0x4d34a1,
                       .climb = {1.45f, 1.3f, 1.15f, 1.05f, 1.0f},
                       .climb_count = 5,
                       .drift = 0.55f},
            .ads_time = 0.22f,
            .view_fov = 0.86f,
            .reload_tac = 2.1f,
            .reload_empty = 2.9f,
            .inspect_time = 3.2f,
            .draw_time = 0.62f,
            .holster_time = 0.4f,
            .hip_pos = {0.118f, -0.185f, -0.3f},
            .hip_rot = {-0.05f, 0.081f, -0.135f},
            .ads_cant = {0.0f, 0.0f, 0.004f},
            .eye_relief = 0.115f,
            .sprint_pos = {0.09f, -0.262f, -0.275f},
            .sprint_rot = {-0.4f, 0.6f, 0.2f},
            .low_ready_pos = {0.112f, -0.28f, -0.289f},
            .low_ready_rot = {-0.46f, 0.125f, -0.09f},
            .sway_scale = 1.0f,
            .bob_scale = 1.0f,
            .mag_len = 0.212f,
        },
    [COD_WEAPON_SMG] =
        {
            .label = "MPX-9",
            .mode_count = 2,
            .modes = {COD_FIRE_AUTO, COD_FIRE_SEMI},
            .rpm = 950.0f,
            .burst_count = 2,
            .burst_rpm = 1100.0f,
            .burst_delay = 0.14f,
            .mag_size = 32,
            .reserve = 224,
            .muzzle_velocity = 400.0f,
            .damage = 24.0f,
            .penetration = 0.45f,
            .dropoff = 0.48f,
            .max_range = 240.0f,
            .drag_k = 0.42f,
            .tracer_every = 4,
            .spread_hip = 2.5f,
            .spread_ads = 0.4f,
            .spread_per_shot = 0.26f,
            .spread_max = 3.9f,
            .spread_decay = 4.4f,
            .recoil = {.pitch = 0.0058f,
                       .yaw = 0.0026f,
                       .kick_back = 0.0135f,
                       .kick_up = 0.0052f,
                       .roll = 0.026f,
                       .punch = 0.24f,
                       .freq = 10.5f,
                       .damping = 0.4f,
                       .pattern_length = 32,
                       .pattern_seed = 0x9ac31f,
                       .climb = {1.3f, 1.18f, 1.08f, 1.0f},
                       .climb_count = 4,
                       .drift = 0.8f},
            .ads_time = 0.185f,
            .view_fov = 0.88f,
            .reload_tac = 1.85f,
            .reload_empty = 2.5f,
            .inspect_time = 2.9f,
            .draw_time = 0.52f,
            .holster_time = 0.34f,
            .hip_pos = {0.111f, -0.163f, -0.288f},
            .hip_rot = {-0.05f, 0.072f, -0.131f},
            .ads_cant = {0.0f, 0.0f, 0.005f},
            .eye_relief = 0.104f,
            .sprint_pos = {0.088f, -0.24f, -0.262f},
            .sprint_rot = {-0.38f, 0.58f, 0.19f},
            .low_ready_pos = {0.108f, -0.252f, -0.276f},
            .low_ready_rot = {-0.44f, 0.125f, -0.085f},
            .sway_scale = 0.92f,
            .bob_scale = 0.95f,
            .mag_len = 0.192f,
        },
    [COD_WEAPON_PISTOL] =
        {
            .label = "P-19",
            .mode_count = 1,
            .modes = {COD_FIRE_SEMI},
            .rpm = 460.0f,
            .burst_count = 1,
            .burst_rpm = 460.0f,
            .burst_delay = 0.1f,
            .mag_size = 17,
            .reserve = 68,
            .muzzle_velocity = 360.0f,
            .damage = 28.0f,
            .penetration = 0.35f,
            .dropoff = 0.42f,
            .max_range = 180.0f,
            .drag_k = 0.46f,
            .tracer_every = 5,
            .spread_hip = 3.1f,
            .spread_ads = 0.5f,
            .spread_per_shot = 0.42f,
            .spread_max = 4.6f,
            .spread_decay = 5.2f,
            .recoil = {.pitch = 0.0125f,
                       .yaw = 0.0032f,
                       .kick_back = 0.012f,
                       .kick_up = 0.0105f,
                       .roll = 0.018f,
                       .punch = 0.3f,
                       .freq = 9.0f,
                       .damping = 0.45f,
                       .pattern_length = 17,
                       .pattern_seed = 0x1f77bc,
                       .climb = {1.0f},
                       .climb_count = 1,
                       .drift = 1.2f},
            .ads_time = 0.16f,
            .view_fov = 0.92f,
            .reload_tac = 1.6f,
            .reload_empty = 2.2f,
            .inspect_time = 2.6f,
            .draw_time = 0.42f,
            .holster_time = 0.3f,
            .hip_pos = {0.115f, -0.15f, -0.34f},
            .hip_rot = {-0.05f, 0.066f, -0.115f},
            .ads_cant = {0.0f, 0.0f, 0.003f},
            .eye_relief = 0.34f,
            .sprint_pos = {0.09f, -0.25f, -0.28f},
            .sprint_rot = {-0.42f, 0.5f, 0.14f},
            .low_ready_pos = {0.1f, -0.26f, -0.32f},
            .low_ready_rot = {-0.44f, 0.105f, -0.07f},
            .sway_scale = 1.15f,
            .bob_scale = 1.1f,
            .mag_len = 0.108f,
        },
};

const CwpDef *cwp_def(u32 weapon) { return &cwp_defs[weapon < COD_WEAPON_COUNT ? weapon : 0]; }

void cwp_build_recoil_pattern(const CwpDef *def, f32 *out) {
  const CwpRecoilDef *r = &def->recoil;
  u32 n = r->pattern_length;
  CodRng rng = cod_rng_make(r->pattern_seed);
  // two out-of-phase wanders make the horizontal read as a learnable snake rather than noise
  f32 phase = cod_rng_float(&rng) * (f32)(2.0 * M_PI);
  f32 phase2 = cod_rng_float(&rng) * (f32)(2.0 * M_PI);
  f32 bias = cod_rng_signed(&rng) * 0.35f;
  for (u32 i = 0; i < n; i++) {
    f32 climb = r->climb[i < r->climb_count ? i : r->climb_count - 1];
    f32 sig = 0.88f + cod_rng_float(&rng) * 0.24f;
    out[i * 2] = r->pitch * climb * sig;
    f32 t = (f32)i / m_maxf(1.0f, (f32)(n - 1));
    f32 snake = m_sinf(phase + t * (f32)M_PI * 2.6f) * 0.75f + m_sinf(phase2 + t * (f32)M_PI * 5.1f) * 0.35f;
    out[i * 2 + 1] = r->yaw * (snake * r->drift * 3.2f + bias + cod_rng_signed(&rng) * 0.25f);
  }
}

// ---- clips ----

hz_internal f32 cwp_ease(u32 e, f32 t) {
  switch (e) {
  case CWP_EASE_LINEAR:
    return t;
  case CWP_EASE_OUT: {
    f32 p = 1.0f - t;
    return 1.0f - p * p * p;
  }
  case CWP_EASE_BACK: {
    f32 p = t - 1.0f, k = 1.4f;
    return 1.0f + p * p * ((k + 1.0f) * p + k);
  }
  default:
    return cwp_smootherstep(0.0f, 1.0f, t);
  }
}

// the key pair around t and the eased blend between them; every key struct starts with CwpKeyHead
hz_internal const CwpKeyHead *cwp_key_at(const void *keys, u32 stride, u32 i) { return (const CwpKeyHead *)((const u8 *)keys + (size_t)i * stride); }

hz_internal void cwp_track(const void *keys, u32 stride, u32 count, f32 t, u32 *ia, u32 *ib, f32 *w) {
  u32 i = 0;
  while (i + 1 < count && cwp_key_at(keys, stride, i + 1)->t <= t)
    i++;
  u32 j = i + 1 < count ? i + 1 : count - 1;
  *ia = i;
  *ib = j;
  *w = 0.0f;
  if (j != i) {
    f32 span = cwp_key_at(keys, stride, j)->t - cwp_key_at(keys, stride, i)->t;
    f32 x = span > 1e-6f ? cwp_clamp01((t - cwp_key_at(keys, stride, i)->t) / span) : 1.0f;
    *w = cwp_ease(cwp_key_at(keys, stride, j)->ease, x);
  }
}

void cwp_clip_sample(const CwpClip *c, f32 t, CwpClipSample *out) {
  out->active = true;
  u32 ia, ib;
  f32 w;
  if (c->weapon_count) {
    cwp_track(c->weapon, sizeof(CwpKeyWeapon), c->weapon_count, t, &ia, &ib, &w);
    out->pos = v3_lerp(c->weapon[ia].p, c->weapon[ib].p, w);
    out->rot = v3_lerp(c->weapon[ia].r, c->weapon[ib].r, w);
  } else {
    out->pos = V3_ZERO;
    out->rot = V3_ZERO;
  }
  if (c->hand_count) {
    cwp_track(c->hand, sizeof(CwpKeyHand), c->hand_count, t, &ia, &ib, &w);
    const CwpKeyHand *a = &c->hand[ia], *b = &c->hand[ib];
    out->hand_pos = v3_lerp(a->p, b->p, w);
    out->hand_finger = v3_lerp(a->finger, b->finger, w);
    out->hand_back = v3_lerp(a->back, b->back, w);
    out->hand_pose = w < 0.5f ? a->pose : b->pose;
    out->hand_weight = m_lerpf(a->weight, b->weight, w);
  } else {
    out->hand_weight = 0.0f;
  }
  if (c->parts_count) {
    cwp_track(c->parts, sizeof(CwpKeyParts), c->parts_count, t, &ia, &ib, &w);
    const CwpKeyParts *a = &c->parts[ia], *b = &c->parts[ib];
    out->mag = m_lerpf(a->mag, b->mag, w);
    out->mag_visible = b->mag_visible > 0.5f || w < 0.5f;
    out->charge = m_lerpf(a->charge, b->charge, w);
    out->bolt = m_lerpf(a->bolt, b->bolt, w);
    out->slide = m_lerpf(a->slide, b->slide, w);
  }
}

typedef struct CwpClipBuilder {
  CwpClip *c;
  const CwpClipNodes *n;
  v3 wrap_finger, wrap_back, mag_finger, mag_back;
} CwpClipBuilder;

hz_internal void cwp_kw(CwpClip *c, f32 t, v3 p, v3 r, u32 ease) { c->weapon[c->weapon_count++] = (CwpKeyWeapon){t, ease, p, r}; }
hz_internal void cwp_kh(CwpClip *c, f32 t, v3 p, v3 finger, v3 back, u32 pose, u32 ease) {
  c->hand[c->hand_count++] = (CwpKeyHand){t, ease, p, finger, back, pose, 1.0f};
}
hz_internal void cwp_kp(CwpClip *c, f32 t, f32 mag, f32 vis, f32 charge, f32 bolt, f32 slide, u32 ease) {
  c->parts[c->parts_count++] = (CwpKeyParts){t, ease, mag, vis, charge, bolt, slide};
}
hz_internal void cwp_ke(CwpClip *c, f32 t, u32 name) { c->events[c->event_count++] = (CwpClipEventKey){t, name}; }

void cwp_build_clips(CwpClip *out, const CwpClipNodes *nodes, const CwpDef *def) {
  v3 wf = nodes->grip_finger, wb = nodes->grip_back;
  v3 mf = {0.1f, 0.72f, -0.68f}, mb = {-0.86f, 0.34f, -0.38f};
  v3 hg = nodes->grip_pos;
  v3 seat = nodes->mag_seat;
  f32 ml = def->mag_len;
  v3 at_mag = {seat.x + 0.012f, seat.y - ml * 0.62f, seat.z + 0.012f};
  v3 below = {seat.x + 0.05f, seat.y - ml * 1.5f, seat.z + 0.09f};
  v3 off = {seat.x + 0.11f, seat.y - ml * 2.0f, seat.z + 0.16f};
  v3 high = {seat.x + 0.006f, seat.y - ml * 0.78f, seat.z + 0.008f};
  v3 seated = {seat.x, seat.y - ml * 0.62f, seat.z};
  v3 charge = nodes->has_charge ? (v3){nodes->charge_rest.x - 0.02f, nodes->charge_rest.y + 0.008f, nodes->charge_rest.z + 0.03f} : V3_ZERO;
  f32 tac = def->reload_tac, emp = def->reload_empty;
  const u32 S = CWP_EASE_SMOOTH, L = CWP_EASE_LINEAR, O = CWP_EASE_OUT, B = CWP_EASE_BACK;

  // ---- tactical ----
  CwpClip *c = &out[CWP_CLIP_RELOAD_TAC];
  *c = (CwpClip){.duration = tac};
  cwp_kw(c, 0.0f, V3_ZERO, V3_ZERO, S);
  cwp_kw(c, 0.12f * tac, (v3){0.014f, -0.026f, 0.03f}, (v3){-0.14f, 0.3f, 0.42f}, S);
  cwp_kw(c, 0.5f * tac, (v3){0.016f, -0.03f, 0.026f}, (v3){-0.1f, 0.34f, 0.5f}, S);
  cwp_kw(c, 0.72f * tac, (v3){0.012f, -0.022f, 0.022f}, (v3){-0.12f, 0.26f, 0.44f}, S);
  cwp_kw(c, 0.78f * tac, (v3){0.008f, -0.008f, 0.014f}, (v3){-0.05f, 0.18f, 0.3f}, B);
  cwp_kw(c, 1.0f, V3_ZERO, V3_ZERO, S);
  cwp_kh(c, 0.0f, hg, wf, wb, CWP_POSE_WRAP, S);
  cwp_kh(c, 0.1f * tac, at_mag, mf, mb, CWP_POSE_PINCH, S);
  cwp_kh(c, 0.2f * tac, at_mag, mf, mb, CWP_POSE_PINCH, S);
  cwp_kh(c, 0.3f * tac, below, mf, mb, CWP_POSE_PINCH, O);
  cwp_kh(c, 0.42f * tac, off, mf, mb, CWP_POSE_OPEN, S);
  cwp_kh(c, 0.56f * tac, off, mf, mb, CWP_POSE_PINCH, S);
  cwp_kh(c, 0.68f * tac, below, mf, mb, CWP_POSE_PINCH, S);
  cwp_kh(c, 0.76f * tac, high, mf, mb, CWP_POSE_PINCH, O);
  cwp_kh(c, 0.8f * tac, seated, mf, mb, CWP_POSE_PINCH, S);
  cwp_kh(c, 0.86f * tac, (v3){seated.x, seated.y - 0.012f, seated.z}, mf, mb, CWP_POSE_OPEN, S);
  cwp_kh(c, 1.0f, hg, wf, wb, CWP_POSE_WRAP, O);
  cwp_kp(c, 0.0f, 0, 1, 0, 0, 0, S);
  cwp_kp(c, 0.16f * tac, 0, 1, 0, 0, 0, S);
  cwp_kp(c, 0.2f * tac, 1, 1, 0, 0, 0, S);
  cwp_kp(c, 0.3f * tac, 1, 1, 0, 0, 0, L);
  cwp_kp(c, 0.34f * tac, 1, 0, 0, 0, 0, S);
  cwp_kp(c, 0.66f * tac, 1, 0, 0, 0, 0, S);
  cwp_kp(c, 0.68f * tac, 1, 1, 0, 0, 0, S);
  cwp_kp(c, 0.79f * tac, 1, 1, 0, 0, 0, O);
  cwp_kp(c, 0.81f * tac, 0, 1, 0, 0, 0, S);
  cwp_kp(c, 1.0f, 0, 1, 0, 0, 0, S);
  cwp_ke(c, 0.02f * tac, CWP_EV_START);
  cwp_ke(c, 0.2f * tac, CWP_EV_MAGOUT);
  cwp_ke(c, 0.34f * tac, CWP_EV_MAGDROP);
  cwp_ke(c, 0.81f * tac, CWP_EV_MAGIN);
  cwp_ke(c, 0.88f * tac, CWP_EV_SLAP);
  cwp_ke(c, 0.995f * tac, CWP_EV_END);

  // ---- empty ----
  c = &out[CWP_CLIP_RELOAD_EMPTY];
  *c = (CwpClip){.duration = emp};
  cwp_kw(c, 0.0f, V3_ZERO, V3_ZERO, S);
  cwp_kw(c, 0.1f * emp, (v3){0.016f, -0.03f, 0.032f}, (v3){-0.16f, 0.34f, 0.46f}, S);
  cwp_kw(c, 0.44f * emp, (v3){0.018f, -0.034f, 0.028f}, (v3){-0.12f, 0.38f, 0.54f}, S);
  cwp_kw(c, 0.7f * emp, (v3){0.014f, -0.026f, 0.024f}, (v3){-0.14f, 0.3f, 0.48f}, S);
  cwp_kw(c, 0.72f * emp, (v3){0.01f, -0.014f, 0.018f}, (v3){-0.06f, 0.24f, 0.38f}, B);
  cwp_kw(c, 0.86f * emp, (v3){0.006f, -0.012f, 0.016f}, (v3){-0.02f, 0.42f, 0.22f}, S);
  cwp_kw(c, 0.92f * emp, (v3){0.004f, -0.006f, 0.022f}, (v3){0.02f, 0.44f, 0.18f}, L);
  cwp_kw(c, 1.0f, V3_ZERO, V3_ZERO, O);
  cwp_kh(c, 0.0f, hg, wf, wb, CWP_POSE_WRAP, S);
  cwp_kh(c, 0.08f * emp, at_mag, mf, mb, CWP_POSE_PINCH, S);
  cwp_kh(c, 0.16f * emp, at_mag, mf, mb, CWP_POSE_PINCH, S);
  cwp_kh(c, 0.26f * emp, below, mf, mb, CWP_POSE_OPEN, O);
  cwp_kh(c, 0.36f * emp, off, mf, mb, CWP_POSE_OPEN, S);
  cwp_kh(c, 0.48f * emp, off, mf, mb, CWP_POSE_PINCH, S);
  cwp_kh(c, 0.58f * emp, below, mf, mb, CWP_POSE_PINCH, S);
  cwp_kh(c, 0.66f * emp, high, mf, mb, CWP_POSE_PINCH, O);
  cwp_kh(c, 0.7f * emp, seated, mf, mb, CWP_POSE_PINCH, S);
  cwp_kh(c, 0.75f * emp, (v3){seated.x, seated.y - 0.01f, seated.z}, mf, mb, CWP_POSE_OPEN, S);
  if (nodes->has_charge) {
    v3 cf = {0.55f, 0.2f, 0.81f}, cb = {-0.2f, 0.94f, -0.27f};
    cwp_kh(c, 0.82f * emp, (v3){charge.x, charge.y, charge.z - 0.01f}, cf, cb, CWP_POSE_PINCH, O);
    cwp_kh(c, 0.87f * emp, charge, cf, cb, CWP_POSE_PINCH, S);
    cwp_kh(c, 0.9f * emp, (v3){charge.x, charge.y, charge.z + 0.07f}, cf, cb, CWP_POSE_PINCH, L);
    cwp_kh(c, 0.93f * emp, (v3){charge.x, charge.y, charge.z + 0.02f}, cf, cb, CWP_POSE_OPEN, O);
  } else {
    v3 pf = {0.7f, -0.3f, 0.65f}, pb = {0.1f, 0.94f, 0.32f};
    cwp_kh(c, 0.82f * emp, (v3){-0.02f, seat.y + 0.06f, seat.z - 0.05f}, pf, pb, CWP_POSE_PINCH, O);
    cwp_kh(c, 0.88f * emp, (v3){-0.02f, seat.y + 0.06f, seat.z - 0.02f}, pf, pb, CWP_POSE_PINCH, L);
    cwp_kh(c, 0.92f * emp, (v3){-0.02f, seat.y + 0.06f, seat.z - 0.06f}, pf, pb, CWP_POSE_OPEN, O);
  }
  cwp_kh(c, 1.0f, hg, wf, wb, CWP_POSE_WRAP, O);
  cwp_kp(c, 0.0f, 0, 1, 0, 1, 1, S);
  cwp_kp(c, 0.12f * emp, 0, 1, 0, 1, 1, S);
  cwp_kp(c, 0.16f * emp, 1, 1, 0, 1, 1, S);
  cwp_kp(c, 0.26f * emp, 1, 1, 0, 1, 1, L);
  cwp_kp(c, 0.3f * emp, 1, 0, 0, 1, 1, S);
  cwp_kp(c, 0.56f * emp, 1, 0, 0, 1, 1, S);
  cwp_kp(c, 0.58f * emp, 1, 1, 0, 1, 1, S);
  cwp_kp(c, 0.69f * emp, 1, 1, 0, 1, 1, O);
  cwp_kp(c, 0.71f * emp, 0, 1, 0, 1, 1, S);
  cwp_kp(c, 0.86f * emp, 0, 1, 0, 1, 1, S);
  cwp_kp(c, 0.9f * emp, 0, 1, 1, 1, 1, L);
  cwp_kp(c, 0.915f * emp, 0, 1, 0, 0, 0, B);
  cwp_kp(c, 1.0f, 0, 1, 0, 0, 0, S);
  cwp_ke(c, 0.02f * emp, CWP_EV_START);
  cwp_ke(c, 0.16f * emp, CWP_EV_MAGOUT);
  cwp_ke(c, 0.3f * emp, CWP_EV_MAGDROP);
  cwp_ke(c, 0.71f * emp, CWP_EV_MAGIN);
  cwp_ke(c, 0.9f * emp, CWP_EV_CHARGE);
  cwp_ke(c, 0.917f * emp, CWP_EV_BOLTRELEASE);
  cwp_ke(c, 0.995f * emp, CWP_EV_END);

  // ---- inspect ----
  f32 insp = def->inspect_time;
  c = &out[CWP_CLIP_INSPECT];
  *c = (CwpClip){.duration = insp};
  cwp_kw(c, 0.0f, V3_ZERO, V3_ZERO, S);
  cwp_kw(c, 0.16f * insp, (v3){-0.03f, -0.012f, 0.075f}, (v3){0.1f, -0.62f, -0.34f}, S);
  cwp_kw(c, 0.34f * insp, (v3){-0.026f, -0.006f, 0.085f}, (v3){-0.05f, -0.78f, -0.5f}, S);
  cwp_kw(c, 0.52f * insp, (v3){0.01f, -0.02f, 0.07f}, (v3){0.22f, 0.5f, 0.9f}, S);
  cwp_kw(c, 0.7f * insp, (v3){0.012f, -0.024f, 0.055f}, (v3){0.3f, 0.62f, 1.15f}, S);
  cwp_kw(c, 0.86f * insp, (v3){-0.006f, -0.01f, 0.03f}, (v3){0.06f, -0.18f, 0.2f}, S);
  cwp_kw(c, 1.0f, V3_ZERO, V3_ZERO, O);
  cwp_kh(c, 0.0f, hg, wf, wb, CWP_POSE_WRAP, S);
  cwp_kh(c, 0.3f * insp, (v3){hg.x - 0.01f, hg.y - 0.01f, hg.z + 0.03f}, wf, wb, CWP_POSE_CLAMP, S);
  cwp_kh(c, 0.55f * insp, (v3){hg.x + 0.01f, hg.y - 0.02f, hg.z - 0.02f}, wf, wb, CWP_POSE_WRAP, S);
  cwp_kh(c, 1.0f, hg, wf, wb, CWP_POSE_WRAP, O);
  cwp_kp(c, 0.0f, 0, 1, 0, 0, 0, S);
  cwp_kp(c, 1.0f, 0, 1, 0, 0, 0, S);
  cwp_ke(c, 0.995f * insp, CWP_EV_END);

  // ---- draw ----
  f32 dt_ = def->draw_time;
  c = &out[CWP_CLIP_DRAW];
  *c = (CwpClip){.duration = dt_};
  cwp_kw(c, 0.0f, (v3){0.05f, -0.3f, 0.14f}, (v3){-0.85f, 0.5f, 0.55f}, S);
  cwp_kw(c, 0.55f * dt_, (v3){0.01f, -0.03f, 0.02f}, (v3){-0.1f, 0.06f, 0.06f}, O);
  cwp_kw(c, 0.78f * dt_, (v3){-0.004f, 0.008f, -0.006f}, (v3){0.04f, -0.02f, -0.02f}, S);
  cwp_kw(c, 1.0f, V3_ZERO, V3_ZERO, O);
  cwp_kh(c, 0.0f, (v3){hg.x - 0.02f, hg.y - 0.09f, hg.z + 0.06f}, wf, wb, CWP_POSE_OPEN, S);
  cwp_kh(c, 0.6f * dt_, hg, wf, wb, CWP_POSE_WRAP, O);
  cwp_kh(c, 1.0f, hg, wf, wb, CWP_POSE_WRAP, S);
  cwp_kp(c, 0.0f, 0, 1, 0, 0, 0, S);
  cwp_ke(c, 0.995f * dt_, CWP_EV_END);

  // ---- holster ----
  f32 ht = def->holster_time;
  c = &out[CWP_CLIP_HOLSTER];
  *c = (CwpClip){.duration = ht};
  cwp_kw(c, 0.0f, V3_ZERO, V3_ZERO, S);
  cwp_kw(c, 0.25f * ht, (v3){0.004f, 0.014f, -0.01f}, (v3){0.08f, -0.04f, -0.05f}, S);
  cwp_kw(c, 1.0f, (v3){0.05f, -0.32f, 0.15f}, (v3){-0.9f, 0.55f, 0.6f}, O);
  cwp_kh(c, 0.0f, hg, wf, wb, CWP_POSE_WRAP, S);
  cwp_kh(c, 1.0f, (v3){hg.x - 0.02f, hg.y - 0.1f, hg.z + 0.07f}, wf, wb, CWP_POSE_OPEN, O);
  cwp_kp(c, 0.0f, 0, 1, 0, 0, 0, S);
  cwp_ke(c, 0.995f * ht, CWP_EV_END);
}
