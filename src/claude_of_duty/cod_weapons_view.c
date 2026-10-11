// Claude of Duty weapons: the viewmodel rig, a stack of additive procedural layers over one base pose.

hz_internal const v3 cwp_shoulder_r = {0.205f, -0.2f, 0.06f};
hz_internal const v3 cwp_shoulder_l = {-0.2f, -0.22f, 0.02f};
hz_internal const f32 cwp_noise_rates[CWP_NOISE_COUNT] = {0.13f, 0.19f, 0.271f, 0.083f, 0.117f, 0.163f};

// ---- noise ----

hz_internal void cwp_noise_init(CwpNoise *n, CodRng *rng) {
  for (u32 i = 0; i < CWP_NOISE_SIZE; i++)
    n->t[i] = cod_rng_signed(rng);
}

hz_internal f32 cwp_noise_at(const CwpNoise *n, f32 x) {
  f32 fl = m_floorf(x);
  f32 fx = x - fl;
  i32 size = CWP_NOISE_SIZE;
  i32 i = (((i32)fl % size) + size) % size;
  f32 a = n->t[(i - 1 + size) % size], b = n->t[i], c = n->t[(i + 1) % size], d = n->t[(i + 2) % size];
  f32 t2 = fx * fx, t3 = t2 * fx;
  return 0.5f * ((2.0f * b) + (-a + c) * fx + (2.0f * a - 5.0f * b + 4.0f * c - d) * t2 + (-a + 3.0f * b - 3.0f * c + d) * t3);
}

// fBm over `oct` octaves; the irrational lacunarity keeps the octaves out of phase
hz_internal f32 cwp_noise_fbm(const CwpNoise *n, f32 x, u32 oct) {
  f32 sum = 0.0f, amp = 1.0f, norm = 0.0f, freq = 1.0f;
  for (u32 i = 0; i < oct; i++) {
    sum += cwp_noise_at(n, x * freq + (f32)i * 37.19f) * amp;
    norm += amp;
    amp *= 0.5f;
    freq *= 2.11713f;
  }
  return sum / (norm != 0.0f ? norm : 1.0f);
}

// ---- springs ----

hz_internal CwpSpring cwp_spring(f32 f, f32 z) { return (CwpSpring){.tune = {f, z}}; }
hz_internal CwpSpring3 cwp_spring3(f32 f, f32 z) { return (CwpSpring3){cwp_spring(f, z), cwp_spring(f, z), cwp_spring(f, z)}; }
hz_internal f32 cwp_spring_step(CwpSpring *s, f32 dt, f32 target) {
  damped_spring_step(&s->s, target, s->tune, dt);
  return s->s.value;
}
hz_internal void cwp_spring_kick(CwpSpring *s, f32 dv) { s->s.velocity += dv; }
hz_internal void cwp_spring_reset(CwpSpring *s) { s->s = (DampedSpring){0}; }
hz_internal void cwp_spring3_fz(CwpSpring3 *s, f32 f, f32 z) { s->a.tune = s->b.tune = s->c.tune = (DampedSpringTuning){f, z}; }
hz_internal void cwp_spring3_kick(CwpSpring3 *s, f32 x, f32 y, f32 z) {
  cwp_spring_kick(&s->a, x);
  cwp_spring_kick(&s->b, y);
  cwp_spring_kick(&s->c, z);
}
hz_internal void cwp_spring3_reset(CwpSpring3 *s) {
  cwp_spring_reset(&s->a);
  cwp_spring_reset(&s->b);
  cwp_spring_reset(&s->c);
}
hz_internal void cwp_spring3_step(CwpSpring3 *s, f32 dt, f32 tx, f32 ty, f32 tz) {
  cwp_spring_step(&s->a, dt, tx);
  cwp_spring_step(&s->b, dt, ty);
  cwp_spring_step(&s->c, dt, tz);
}

// ---- hand basis ----

quat cwp_hand_basis(v3 finger, v3 back) {
  v3 z = v3_normalize(v3_negate(finger)); // hand +Z
  v3 y = v3_sub(back, v3_scale(z, v3_dot(back, z)));
  if (v3_lengthsq(y) < 1e-8f)
    y = (v3){-z.y * z.x, 1.0f - z.y * z.y, -z.y * z.z};
  y = v3_normalize(y); // hand +Y
  v3 x = v3_normalize(v3_cross(y, z)); // hand +X
  return quat_from_m3((m3){{{x.x, x.y, x.z}, {y.x, y.y, y.z}, {z.x, z.y, z.z}}});
}

// the camera's yaw and pitch Euler YXZ
hz_internal void cwp_yaw_pitch(quat q, f32 *yaw, f32 *pitch) {
  m3 m = quat_to_m3(q);
  f32 m13 = m.m[2][0], m23 = m.m[2][1], m33 = m.m[2][2], m11 = m.m[0][0], m31 = m.m[0][2];
  *pitch = m_asinf(-m_clampf(m23, -1.0f, 1.0f));
  *yaw = m_absf(m23) < 0.9999999f ? m_atan2f(m13, m33) : m_atan2f(-m31, m11);
}

// ---- lifecycle ----

void cwp_view_reset(CwpView *v) {
  cwp_spring3_reset(&v->rec_pos);
  cwp_spring3_reset(&v->rec_rot);
  cwp_spring3_reset(&v->settle);
  v->bolt_cycle = 0.0f;
  v->bolt_hold = 0.0f;
}

void cwp_view_init(CwpView *v, CodRng rng) {
  *v = (CwpView){0};
  v->rng = rng;
  v->clip = CWP_CLIP_NONE;
  v->lag = cwp_spring3(5.4f, 0.46f);
  v->lag_rot = cwp_spring3(6.2f, 0.42f);
  v->rec_pos = cwp_spring3(9.0f, 0.42f);
  v->rec_rot = cwp_spring3(9.0f, 0.42f);
  v->settle = cwp_spring3(2.2f, 0.7f);
  v->jump = cwp_spring(5.5f, 0.5f);
  v->land = cwp_spring(7.5f, 0.55f);
  // the tables are built from the stream in order, then smoothed once so the low octaves are gentle
  for (u32 k = 0; k < CWP_NOISE_COUNT; k++) {
    CwpNoise *n = &v->noise[k];
    cwp_noise_init(n, &v->rng);
    tctx_temp_allocator_scope(ta, NULL) {
      f32 *tmp = ALLOC_ARRAY_NO_ZERO(&ta.allocator, f32, CWP_NOISE_SIZE);
      for (u32 i = 0; i < CWP_NOISE_SIZE; i++)
        tmp[i] = (n->t[(i + CWP_NOISE_SIZE - 1) % CWP_NOISE_SIZE] + n->t[i] * 2.0f + n->t[(i + 1) % CWP_NOISE_SIZE]) * 0.25f;
      mem_cpy(n->t, tmp, sizeof(n->t));
    }
  }
  v->rig_rot = quat_identity();
  v->hand_rot_r = quat_identity();
  v->hand_rot_l = quat_identity();
  v->mag_rot = quat_identity();
  v->mag_visible = true;
}

void cwp_view_play(CwpView *v, i32 clip) {
  v->clip = clip;
  v->clip_t = 0.0f;
  v->clip_prev_t = -1.0f;
}

void cwp_view_stop_clip(CwpView *v) {
  v->clip = CWP_CLIP_NONE;
  v->res.active = false;
  v->res.hand_weight = 0.0f;
}

void cwp_view_jump(CwpView *v) { cwp_spring_kick(&v->jump, -1.2f); }
void cwp_view_land(CwpView *v, f32 speed) { cwp_spring_kick(&v->land, m_clampf(speed * 0.45f, 0.4f, 3.4f)); }

void cwp_view_add_recoil(CwpView *v, const CwpDef *def, f32 pitch, f32 yaw, b32 first) {
  const CwpRecoilDef *r = &def->recoil;
  f32 ads = v->ads_t;
  // aiming braces the weapon: less travel, faster return
  f32 scale = m_lerpf(1.0f, 0.54f, ads) * (first ? 1.18f : 1.0f);
  f32 jitter = 0.86f + cod_rng_float(&v->rng) * 0.3f;
  cwp_spring3_fz(&v->rec_pos, r->freq, r->damping);
  cwp_spring3_fz(&v->rec_rot, r->freq * 0.92f, r->damping);
  // a velocity impulse v0 on a spring of angular frequency w peaks at about v0/w, so the amplitudes below are real metres and radians
  f32 wp = (f32)(2.0 * M_PI) * r->freq;
  f32 wr = (f32)(2.0 * M_PI) * r->freq * 0.92f;
  f32 sx = cod_rng_signed(&v->rng);
  cwp_spring3_kick(&v->rec_pos, sx * r->kick_back * 0.2f * scale * wp, r->kick_up * scale * jitter * wp, r->kick_back * scale * jitter * wp);
  f32 ry = cod_rng_signed(&v->rng);
  f32 rz = cod_rng_signed(&v->rng);
  cwp_spring3_kick(&v->rec_rot, (pitch * 5.5f + r->pitch * 1.4f) * scale * jitter * wr, (-yaw * 4.5f - ry * r->yaw * 0.8f) * scale * wr,
                   (rz * 0.4f + 0.6f) * r->roll * scale * wr);
  // slow settling drift after a burst: the muzzle keeps wandering a little
  f32 ws = (f32)(2.0 * M_PI) * v->settle.a.tune.freq;
  f32 s0 = cod_rng_signed(&v->rng);
  f32 s2 = cod_rng_signed(&v->rng);
  cwp_spring3_kick(&v->settle, s0 * 0.0012f * scale * ws, 0.0018f * scale * ws, s2 * 0.003f * scale * ws);
  v->bolt_cycle = 1.0f;
}

// ---- the frame ----

hz_internal v3 cwp_v3_lerp(v3 a, v3 b, f32 t) { return v3_lerp(a, b, t); }

void cwp_view_update(CwpView *v, const CwpModel *w, const CwpDef *def, const CwpClip *clips, const CwpViewInput *s, f32 dt,
                     CwpClipEventFn on_event, void *user) {
  // a non-positive or absurd dt would integrate the whole stack backwards
  dt = dt > 0.0f ? (dt < 0.1f ? dt : 0.1f) : 0.0f;

  // the angular velocity of the camera drives the lag layer; a teleport must not throw the gun off screen
  f32 yaw, pitch;
  cwp_yaw_pitch(s->cam_rot, &yaw, &pitch);
  if (v->has_prev && dt > 1e-5f) {
    f32 dy = m_wrap_pi(yaw - v->prev_yaw) / dt;
    f32 dp = m_wrap_pi(pitch - v->prev_pitch) / dt;
    v->ang_yaw = m_damp(v->ang_yaw, m_clampf(dy, -9.0f, 9.0f), 18.0f, dt);
    v->ang_pitch = m_damp(v->ang_pitch, m_clampf(dp, -9.0f, 9.0f), 18.0f, dt);
  } else {
    v->ang_yaw = 0.0f;
    v->ang_pitch = 0.0f;
  }
  v->prev_yaw = yaw;
  v->prev_pitch = pitch;
  v->has_prev = true;

  // blends: a linear rate with a smootherstep shaping, a spring here reads as mushy
  f32 ads_rate = 1.0f / m_maxf(0.05f, def->ads_time);
  b32 want_ads = (v->clip != CWP_CLIP_NONE && v->clip != CWP_CLIP_DRAW) ? false : s->ads;
  v->ads_t = cwp_clamp01(v->ads_t + (want_ads ? ads_rate : -ads_rate * 1.25f) * dt);
  f32 ads = cwp_smootherstep(0.0f, 1.0f, v->ads_t);
  f32 sprint_target = s->sprint && v->clip == CWP_CLIP_NONE ? 1.0f : 0.0f;
  v->sprint_t = m_damp(v->sprint_t, sprint_target, 9.0f, dt);
  v->low_ready_t = m_damp(v->low_ready_t, s->low_ready ? 1.0f : 0.0f, 8.0f, dt);
  v->trigger_t = m_damp(v->trigger_t, s->trigger ? 1.0f : 0.0f, 26.0f, dt);

  // base pose: hip, replaced by the sprint and low-ready poses
  v3 base_pos = def->hip_pos;
  quat base_q = cwp_quat_euler_xyz(def->hip_rot);
  if (v->sprint_t > 1e-3f) {
    base_pos = cwp_v3_lerp(base_pos, def->sprint_pos, v->sprint_t);
    base_q = quat_slerp(base_q, cwp_quat_euler_xyz(def->sprint_rot), v->sprint_t);
  }
  if (v->low_ready_t > 1e-3f) {
    base_pos = cwp_v3_lerp(base_pos, def->low_ready_pos, v->low_ready_t);
    base_q = quat_slerp(base_q, cwp_quat_euler_xyz(def->low_ready_rot), v->low_ready_t);
  }
  // the ADS pose is solved, not authored: the translation that puts the sight node on the camera axis at the eye relief
  if (ads > 1e-4f) {
    quat ads_q = cwp_quat_euler_xyz(def->ads_cant);
    v3 sight_local = quat_rotate(ads_q, w->sight);
    v3 ads_pos = v3_sub((v3){0.0f, 0.0f, -def->eye_relief}, sight_local);
    base_pos = cwp_v3_lerp(base_pos, ads_pos, ads);
    base_q = quat_slerp(base_q, ads_q, ads);
  }

  // sway: layered incommensurate noise, so idle never visibly loops
  f32 sway_scale = def->sway_scale * m_lerpf(1.0f, 0.22f, ads) * m_lerpf(1.0f, 1.5f, v->sprint_t);
  v->noise_t += dt;
  f32 t = v->noise_t;
  const f32 *rates = cwp_noise_rates;
  const CwpNoise *n = v->noise;
  f32 swx = cwp_noise_fbm(&n[0], t * rates[0], 3) * 0.55f + cwp_noise_fbm(&n[3], t * rates[3] * 2.3f, 2) * 0.45f;
  f32 swy = cwp_noise_fbm(&n[1], t * rates[1], 3) * 0.55f + cwp_noise_fbm(&n[4], t * rates[4] * 2.1f, 2) * 0.45f;
  f32 swz = cwp_noise_fbm(&n[2], t * rates[2], 2) * 0.6f + cwp_noise_fbm(&n[5], t * rates[5] * 1.7f, 2) * 0.4f;
  f32 breath = m_sinf(t * 1.38f) * 0.5f + m_sinf(t * 0.61f + 1.1f) * 0.25f;
  f32 px = swx * 0.0075f * sway_scale;
  f32 py = (swy * 0.006f + breath * 0.0022f) * sway_scale;
  f32 pz = swz * 0.004f * sway_scale;
  f32 rx = (swy * 0.021f + breath * 0.006f) * sway_scale;
  f32 ry = swx * 0.028f * sway_scale;
  f32 rz = swz * 0.017f * sway_scale;

  // movement bob: stride driven, scaled by speed and stance
  f32 speed = s->speed;
  f32 bob_amt = def->bob_scale * cwp_clamp01(speed / 4.2f) * m_lerpf(1.0f, 0.28f, ads) * (s->airborne ? 0.25f : 1.0f);
  if (speed > 0.05f) {
    v->bob_phase += dt * (3.1f + speed * 0.72f) * (s->sprint ? 1.05f : 1.0f);
    if (v->bob_phase > (f32)(2.0 * M_PI) * 64.0f)
      v->bob_phase -= (f32)(2.0 * M_PI) * 64.0f;
  }
  f32 bp = v->bob_phase;
  px += m_sinf(bp) * 0.0165f * bob_amt;
  py += (m_absf(m_cosf(bp)) - 0.6f) * 0.0125f * bob_amt;
  pz += m_sinf(bp * 2.0f) * 0.0055f * bob_amt;
  rz += m_sinf(bp) * 0.031f * bob_amt;
  rx += m_cosf(bp * 2.0f) * 0.014f * bob_amt;
  ry += m_sinf(bp + 0.6f) * 0.019f * bob_amt;

  // weapon lag: the gun trails camera rotation on a spring, overshoots, settles
  f32 lag_scale = m_lerpf(1.0f, 0.42f, ads);
  cwp_spring3_step(&v->lag, dt, m_clampf(-v->ang_yaw * 0.019f, -0.05f, 0.05f) * lag_scale,
                   m_clampf(v->ang_pitch * 0.014f, -0.04f, 0.04f) * lag_scale,
                   m_clampf(-m_absf(v->ang_yaw) * 0.006f, -0.03f, 0.03f) * lag_scale);
  cwp_spring3_step(&v->lag_rot, dt, m_clampf(-v->ang_pitch * 0.075f, -0.24f, 0.24f) * lag_scale,
                   m_clampf(v->ang_yaw * 0.085f, -0.3f, 0.3f) * lag_scale, m_clampf(-v->ang_yaw * 0.055f, -0.2f, 0.2f) * lag_scale);
  px += v->lag.a.s.value;
  py += v->lag.b.s.value;
  pz += v->lag.c.s.value;
  rx += v->lag_rot.a.s.value;
  ry += v->lag_rot.b.s.value;
  rz += v->lag_rot.c.s.value;

  // recoil and settle
  cwp_spring3_step(&v->rec_pos, dt, 0.0f, 0.0f, 0.0f);
  cwp_spring3_step(&v->rec_rot, dt, 0.0f, 0.0f, 0.0f);
  cwp_spring3_step(&v->settle, dt, 0.0f, 0.0f, 0.0f);
  px += v->rec_pos.a.s.value;
  py += v->rec_pos.b.s.value;
  pz += v->rec_pos.c.s.value;
  rx += v->rec_rot.a.s.value + v->settle.b.s.value;
  ry += v->rec_rot.b.s.value + v->settle.a.s.value;
  rz += v->rec_rot.c.s.value + v->settle.c.s.value;

  // jump and land
  cwp_spring_step(&v->jump, dt, 0.0f);
  cwp_spring_step(&v->land, dt, 0.0f);
  py -= v->land.s.value * 0.014f + v->jump.s.value * 0.006f;
  rx -= v->land.s.value * 0.05f;

  // the clip: sampled first, its beats may start another clip (a holster ends into the draw)
  CwpClipSample *res = &v->res;
  if (v->clip != CWP_CLIP_NONE) {
    i32 played = v->clip;
    const CwpClip *c = &clips[played];
    v->clip_t += dt;
    f32 tt = m_clampf(v->clip_t, 0.0f, c->duration);
    cwp_clip_sample(c, tt, res);
    for (u32 e = 0; e < c->event_count; e++) {
      const CwpClipEventKey *ev = &c->events[e];
      if (ev->t > v->clip_prev_t && ev->t <= tt && on_event)
        on_event(user, ev->name, played);
    }
    v->clip_prev_t = tt;
    px += res->pos.x;
    py += res->pos.y;
    pz += res->pos.z;
    rx += res->rot.x;
    ry += res->rot.y;
    rz += res->rot.z;
    if (v->clip_t >= c->duration)
      cwp_view_stop_clip(v);
  }

  // compose
  v->rig_pos = (v3){base_pos.x + px, base_pos.y + py, base_pos.z + pz};
  v->rig_rot = quat_mul(base_q, cwp_quat_euler_xyz((v3){rx, ry, rz}));
  v->fov_scale = m_lerpf(1.0f, def->view_fov, ads);
  v->trigger_out = v->trigger_t;

  // hands: the shoulders are body fixed, so the camera-space anchors are re-based into rig space
  quat inv = quat_conjugate(v->rig_rot);
  v->shoulder_r = quat_rotate(inv, v3_sub(cwp_shoulder_r, v->rig_pos));
  v->shoulder_l = quat_rotate(inv, v3_sub(cwp_shoulder_l, v->rig_pos));
  v3 fr = w->grip_r.finger.x == 0.0f && w->grip_r.finger.y == 0.0f && w->grip_r.finger.z == 0.0f ? (v3){0.0f, -0.35f, -0.94f} : w->grip_r.finger;
  v3 br = w->grip_r.back.x == 0.0f && w->grip_r.back.y == 0.0f && w->grip_r.back.z == 0.0f ? (v3){0.95f, 0.25f, 0.18f} : w->grip_r.back;
  v->hand_pos_r = w->grip_r.pos;
  v->hand_rot_r = cwp_hand_basis(fr, br);
  v3 pos = w->grip_l.pos;
  v3 fl = w->grip_l.finger.x == 0.0f && w->grip_l.finger.y == 0.0f && w->grip_l.finger.z == 0.0f ? (v3){0.82f, 0.5f, -0.28f} : w->grip_l.finger;
  v3 bl = w->grip_l.back.x == 0.0f && w->grip_l.back.y == 0.0f && w->grip_l.back.z == 0.0f ? (v3){-0.5f, 0.32f, -0.8f} : w->grip_l.back;
  u32 pose = v->hand_pose_rest;
  if (res->active && res->hand_weight > 0.5f) {
    pos = res->hand_pos;
    fl = res->hand_finger;
    bl = res->hand_back;
    pose = res->hand_pose;
  }
  v->hand_pos_l = pos;
  v->hand_rot_l = cwp_hand_basis(fl, bl);
  v->hand_pose_l = pose;

  // moving parts: a fast rearward stroke and a slightly slower return
  if (v->bolt_cycle > 0.0f) {
    f32 cycle = m_maxf(0.045f, (60.0f / def->rpm) * 0.62f);
    v->bolt_cycle = m_maxf(0.0f, v->bolt_cycle - dt / cycle);
  }
  f32 cyc = v->bolt_cycle;
  f32 stroke = cyc > 0.55f ? (1.0f - cyc) / 0.45f : cyc / 0.55f;
  f32 clip_bolt = res->active ? res->bolt : 0.0f;
  f32 bolt_off = m_maxf(stroke, m_maxf(v->bolt_hold, clip_bolt * v->bolt_hold));
  v->bolt_pos = v3_add(w->bolt_rest.pos, v3_scale(w->bolt_travel, bolt_off));
  v->slide_pos = v3_add(w->slide_rest.pos, v3_scale(w->slide_travel, bolt_off));
  f32 pull = res->active ? res->charge : 0.0f;
  v->charge_pos = v3_add(w->charge_rest.pos, v3_scale(w->charge_pull, pull));
  v->trigger_rot = w->trigger_pull * v->trigger_t;

  // the magazine: seated, in the support hand, or hidden
  f32 in_hand = res->active ? res->mag : 0.0f;
  v->mag_visible = res->active ? res->mag_visible : true;
  quat seat_q = cwp_quat_euler_xyz(w->mag_seat.rot);
  if (in_hand > 1e-4f) {
    // the hand target is a wrist, so the magazine is offset into the palm (62 mm along the hand's -Z) and gripped by its spine
    v3 held = v3_add(v->hand_pos_l, quat_rotate(v->hand_rot_l, (v3){0.0f, w->mag_len * 0.62f, -0.062f}));
    v->mag_pos = cwp_v3_lerp(w->mag_seat.pos, held, in_hand);
    v->mag_rot = quat_slerp(seat_q, v->hand_rot_l, in_hand);
  } else {
    v->mag_pos = w->mag_seat.pos;
    v->mag_rot = seat_q;
  }
}
