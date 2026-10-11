// Claude of Duty audio, weapon voices: profiles, the shot with its distance mix, whizz and dry fire.

typedef enum CasProfileId {
  CAS_PROF_RIFLE = 0,
  CAS_PROF_AK,
  CAS_PROF_SMG,
  CAS_PROF_PISTOL,
  CAS_PROF_SHOTGUN,
  CAS_PROF_SNIPER,
  CAS_PROF_LMG,
  CAS_PROF_SUPPRESSED,
  CAS_PROF_COUNT,
} CasProfileId;

typedef struct CasProfile {
  const char *name;
  f32 level;
  f32 body_f, body_f2, body_decay, sub_f, sub_decay;
  f32 crack_f, crack_q, crack_decay, drive, asym;
  f32 mid_f, mid_decay, tail_decay, tail_f, tail_end_f;
  f32 mech_delay, mech_level, mech0, mech1, mech2;
  f32 send;
  u32 pellets;
  b32 suppressed;
} CasProfile;

static const CasProfile CAS_PROFILES[CAS_PROF_COUNT] = {
    {"rifle", 1.0f, 148, 56, 0.085f, 62, 0.12f, 2450, 0.95f, 0.055f, 6, 0.35f, 780, 0.05f, 0.3f, 5200, 700, 0.028f, 0.42f, 1880, 3260, 5400, 0.5f, 0, false},
    {"ak", 1.1f, 124, 46, 0.105f, 52, 0.15f, 1780, 0.9f, 0.07f, 7.5f, 0.5f, 640, 0.06f, 0.42f, 4200, 560, 0.034f, 0.55f, 1420, 2650, 4300, 0.55f, 0, false},
    {"smg", 0.84f, 172, 72, 0.06f, 78, 0.08f, 3050, 1.05f, 0.04f, 5, 0.3f, 900, 0.035f, 0.19f, 6200, 900, 0.021f, 0.5f, 2200, 3900, 6300, 0.44f, 0, false},
    {"pistol", 0.74f, 186, 84, 0.05f, 92, 0.07f, 2750, 1.15f, 0.035f, 4.5f, 0.28f, 950, 0.03f, 0.16f, 6800, 1000, 0.038f, 0.46f, 2450, 4200, 6900, 0.4f, 0, false},
    {"shotgun", 1.18f, 108, 40, 0.13f, 44, 0.19f, 1450, 0.7f, 0.09f, 9, 0.6f, 520, 0.08f, 0.5f, 3600, 460, 0.16f, 0.7f, 980, 1760, 3050, 0.6f, 6, false},
    {"sniper", 1.3f, 96, 34, 0.16f, 38, 0.24f, 1320, 0.8f, 0.11f, 10, 0.55f, 470, 0.1f, 0.95f, 3300, 380, 0.19f, 0.65f, 1150, 2050, 3400, 0.72f, 0, false},
    {"lmg", 1.14f, 118, 44, 0.11f, 50, 0.16f, 1920, 0.85f, 0.075f, 8, 0.45f, 610, 0.065f, 0.5f, 4000, 520, 0.03f, 0.6f, 1330, 2480, 4100, 0.58f, 0, false},
    {"suppressed", 0.5f, 132, 64, 0.055f, 70, 0.07f, 900, 0.6f, 0.03f, 2.5f, 0.2f, 430, 0.05f, 0.1f, 1800, 400, 0.019f, 0.85f, 2100, 3700, 5900, 0.18f, 0, true},
};

// the round-robin timbre slot, derived from the profile so every clip set shares one table
typedef struct CasRr {
  f32 body, crack, crack_q, tail, drive, mid, level, mech, tilt;
} CasRr;

hz_internal CasRr cas_rr_slot(u32 profile, u32 slot) {
  CodRng r = cod_rng_make(0x57A70000u + profile * 977u);
  CasRr v = {0};
  for (u32 i = 0; i <= slot; i++) {
    v = (CasRr){.body = cas_semis(cod_rng_range(&r, -1.1f, 1.1f)),
                .crack = cas_semis(cod_rng_range(&r, -1.7f, 1.7f)),
                .crack_q = cod_rng_range(&r, 0.85f, 1.2f),
                .tail = cod_rng_range(&r, 0.86f, 1.18f),
                .drive = cod_rng_range(&r, 0.85f, 1.2f),
                .mid = cas_semis(cod_rng_range(&r, -2.0f, 2.0f)),
                .level = cod_rng_range(&r, 0.93f, 1.07f),
                .mech = cod_rng_range(&r, 0.8f, 1.25f),
                .tilt = cod_rng_range(&r, -2.5f, 2.5f)};
  }
  return v;
}

// the seconds a shot at `dist` can last: the longest layer's envelope with the round robin and jitter at their maxima
hz_internal f32 cas_shot_seconds(const CasProfile *p, f32 dist) {
  const f32 far = 1.0f - cas_clamp(1.0f - dist / 42.0f, 0.0f, 1.0f);
  f32 end = 0.6f;
  end = m_maxf(end, p->tail_decay * 1.27f * (1.0f + far * 1.6f) + 0.01f);
  if (far > 0.12f) {
    const f32 boom = 0.28f + dist * 0.0055f;
    end = m_maxf(end, 0.012f + dist * 0.0004f + boom + 0.01f);
    end = m_maxf(end, cas_clamp(dist * 0.0022f, 0.012f, 0.12f) + 0.004f + 0.12f + dist * 0.001f + 0.01f);
  }
  end = m_maxf(end, p->mech_delay * 2.1f * 1.2f + 0.15f);
  return end + 0.05f;
}

// transient, body and sub, crack, mid, tail, mechanical, distant boom, pellets
hz_internal CasSyn cas_voice_shot(Allocator *al, u64 seed, u32 profile_id, f32 dist, b32 fp, u32 slot) {
  const CasProfile *P = &CAS_PROFILES[profile_id];
  CasSyn s = cas_syn_begin(al, seed, (u32)m_ceilf(cas_shot_seconds(P, dist) * (f32)CAS_SR));
  const CasRr v = cas_rr_slot(profile_id, slot % 6u);
  const f32 jB = v.body * cas_semis(cas_range(&s, -0.45f, 0.45f));
  const f32 jC = v.crack * cas_semis(cas_range(&s, -0.8f, 0.8f));
  const f32 jT = v.tail * cas_range(&s, 0.94f, 1.07f);
  const f32 jL = v.level * cas_range(&s, 0.95f, 1.05f);
  const f32 near_v = cas_clamp(1.0f - dist / 42.0f, 0.0f, 1.0f);
  const f32 nearP = m_powf(near_v, 0.7f);
  const f32 far = 1.0f - near_v;
  const f32 t0 = 0.0f;

  // 1. transient: the pressure step
  if (nearP > 0.05f) {
    CasWin w = cas_win(&s, t0, 0.0075f + 0.002f);
    cas_win_noise(&s, w, CAS_NOISE_WHITE, cas_range(&s, 0.9f, 1.3f));
    cas_win_svf_c(w, PG_SVF_HIGHPASS, 2600.0f, 0.6f);
    cas_win_peaking(w, 6200.0f * jC, 1.1f, 8.0f + v.tilt);
    CasParam g = cas_param(&s, 0.0f, 3);
    cas_hit(&g, t0, 0.9f * nearP * jL * (P->suppressed ? 0.35f : 1.0f), 0.0075f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
    CasWin c = cas_win(&s, t0, 0.004f + 0.002f);
    cas_win_osc_c(&s, c, PG_OSC_TRI, 1750.0f * jC);
    CasParam cg = cas_param(&s, 0.0f, 3);
    cas_hit(&cg, t0, 0.35f * nearP * jL, 0.004f);
    cas_win_gain(&s, c, &cg);
    cas_win_mix(&s, c, 1.0f);
  }

  // 2. body and sub
  {
    const f32 body_level = (0.85f + far * 0.5f) * jL * P->level;
    const f32 decay = P->body_decay * cas_range(&s, 0.9f, 1.15f);
    const f32 env_end = 0.0012f + decay + 0.002f;
    CasWin w1 = cas_win(&s, t0, env_end);
    cas_win_osc_sweep(&s, w1, PG_OSC_SINE, t0, P->body_f * jB, P->body_f2 * jB, P->body_decay * 1.4f);
    CasWin w2 = cas_win(&s, t0, env_end);
    cas_win_osc_sweep(&s, w2, PG_OSC_TRI, t0, P->body_f * jB * 0.5f, P->body_f2 * jB * 0.55f, P->body_decay * 1.6f);
    for (u32 i = 0; i < w1.len; i++)
      w1.x[i] += w2.x[i];
    CasParam bg = cas_param(&s, 0.0f, 5);
    cas_ad(&bg, t0, body_level, 0.0012f, decay);
    cas_win_gain(&s, w1, &bg);
    cas_win_shape(&s, w1, cas_curve(&s, P->drive * v.drive * 0.5f, P->asym), 2);
    cas_win_svf_c(w1, PG_SVF_LOWPASS, cas_lerp(2200.0f, 700.0f, far), 0.9f);
    cas_win_mix(&s, w1, 1.0f);

    const f32 sub_env = 0.004f + P->sub_decay * 1.3f + 0.002f;
    CasWin sw = cas_win(&s, t0, sub_env);
    cas_win_osc_sweep(&s, sw, PG_OSC_SINE, t0, P->sub_f * jB * 1.5f, P->sub_f * jB * 0.8f, P->sub_decay);
    CasParam sg = cas_param(&s, 0.0f, 5);
    cas_ad(&sg, t0, (0.5f + far * 0.55f) * P->level, 0.004f, P->sub_decay * 1.3f);
    cas_win_gain(&s, sw, &sg);
    cas_win_mix(&s, sw, 1.0f);
  }

  // 3. crack: resonant noise driven into saturation
  if (nearP > 0.03f) {
    const f32 decay = P->crack_decay * cas_range(&s, 0.85f, 1.2f);
    const f32 env_end = 0.0015f + decay + 0.002f;
    const f32 rate = cas_range(&s, 0.85f, 1.25f);
    CasWin w = cas_win(&s, t0, cas_src_len(P->crack_decay * 3.0f + 0.05f, rate, env_end));
    cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
    cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, t0, P->crack_f * jC * 1.35f, P->crack_f * jC * 0.8f, P->crack_decay * 2.0f,
                      P->crack_q * v.crack_q);
    cas_win_peaking(w, P->crack_f * jC * 1.9f, 1.6f, 6.0f + v.tilt);
    cas_win_shape(&s, w, cas_curve(&s, P->drive * v.drive, P->asym * 0.6f), 2);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 1.05f * nearP * jL * P->level, 0.0015f, decay);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }

  // 4. mid body
  {
    const f32 rate = cas_range(&s, 0.8f, 1.25f);
    CasWin w = cas_win(&s, t0, cas_src_len(P->mid_decay * 4.0f + 0.05f, rate, 0.002f + P->mid_decay * 1.4f + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_PINK, rate);
    cas_win_svf_c(w, PG_SVF_BANDPASS, P->mid_f * v.mid, 1.1f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, (0.5f + far * 0.35f) * jL * P->level, 0.002f, P->mid_decay * 1.4f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }

  // 5. tail: what the room hears
  {
    const f32 tail = P->tail_decay * jT * (1.0f + far * 1.6f);
    const f32 rate = cas_range(&s, 0.7f, 1.15f);
    CasWin w = cas_win(&s, t0, cas_src_len(tail * 1.3f + 0.05f, rate, 0.006f + tail + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_PINK, rate);
    cas_win_svf_c(w, PG_SVF_HIGHPASS, cas_lerp(160.0f, 70.0f, far), 0.7f);
    cas_win_svf_sweep(&s, w, PG_SVF_LOWPASS, t0, P->tail_f * cas_lerp(1.0f, 0.35f, far), P->tail_end_f * cas_lerp(1.0f, 0.6f, far),
                      tail, 0.6f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, (0.42f + far * 0.5f) * jL * P->level, 0.006f, tail);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }

  // 6. mechanical: the bolt, its return and the gas hiss, audible only close up
  if (dist < 14.0f && P->mech_level > 0.0f) {
    const f32 md = P->mech_delay * cas_range(&s, 0.85f, 1.2f);
    const f32 lvl = P->mech_level * v.mech * (fp ? 1.0f : 0.6f) * cas_clamp(1.0f - dist / 14.0f, 0.15f, 1.0f);
    const f32 f0 = P->mech0 * cas_range(&s, 0.96f, 1.05f);
    const f32 f1 = P->mech1 * cas_range(&s, 0.96f, 1.05f);
    const f32 f2 = P->mech2 * cas_range(&s, 0.96f, 1.05f);
    cas_struck4(&s, t0 + md, 0.0035f, 3, (CasPartial){f0, 26.0f, 0.5f * lvl, 0.055f}, (CasPartial){f1, 20.0f, 0.34f * lvl, 0.035f},
                (CasPartial){f2, 14.0f, 0.2f * lvl, 0.02f}, (CasPartial){0});
    cas_struck4(&s, t0 + md * 2.1f, 0.003f, 2, (CasPartial){P->mech0 * 0.88f, 18.0f, 0.3f * lvl, 0.04f},
                (CasPartial){P->mech1 * 1.12f, 12.0f, 0.16f * lvl, 0.022f}, (CasPartial){0}, (CasPartial){0});
    const f32 rate = cas_range(&s, 1.0f, 1.4f);
    CasWin w = cas_win(&s, t0 + md * 0.6f, cas_src_len(0.12f, rate, 0.006f + 0.05f + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
    cas_win_svf_c(w, PG_SVF_BANDPASS, 4200.0f * cas_range(&s, 0.9f, 1.1f), 1.4f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0 + md * 0.6f, 0.12f * lvl, 0.006f, 0.05f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }

  // 7. distant rolling boom and the terrain bounce
  if (far > 0.12f) {
    const f32 boom = 0.28f + dist * 0.0055f;
    const f32 rate = cas_range(&s, 0.6f, 1.0f);
    const f32 attack = 0.012f + dist * 0.0004f;
    CasWin w = cas_win(&s, t0, cas_src_len(boom * 1.4f + 0.05f, rate, attack + boom + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_BROWN, rate);
    cas_win_svf_sweep(&s, w, PG_SVF_LOWPASS, t0, 620.0f, 190.0f, boom, 0.8f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 0.95f * far * far * P->level, attack, boom);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);

    const f32 bounce_t = t0 + cas_clamp(dist * 0.0022f, 0.012f, 0.12f);
    const f32 brate = cas_range(&s, 0.6f, 0.9f);
    CasWin b = cas_win(&s, bounce_t, cas_src_len(0.4f, brate, 0.004f + 0.12f + dist * 0.001f + 0.002f));
    cas_win_noise(&s, b, CAS_NOISE_PINK, brate);
    cas_win_svf_c(b, PG_SVF_LOWPASS, 900.0f, 0.7f);
    CasParam bg = cas_param(&s, 0.0f, 5);
    cas_ad(&bg, bounce_t, 0.3f * far, 0.004f, 0.12f + dist * 0.001f);
    cas_win_gain(&s, b, &bg);
    cas_win_mix(&s, b, 1.0f);
  }

  // shotgun pellet spatter
  if (P->pellets && nearP > 0.2f) {
    for (u32 i = 0; i < P->pellets; i++) {
      const f32 pt = t0 + cas_range(&s, 0.0004f, 0.006f);
      const f32 rate = cas_range(&s, 0.9f, 1.4f);
      const f32 decay = cas_range(&s, 0.004f, 0.014f);
      CasWin w = cas_win(&s, pt, cas_src_len(0.05f, rate, decay + 0.002f));
      cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
      cas_win_svf_c(w, PG_SVF_BANDPASS, cas_range(&s, 2600.0f, 6200.0f), 1.8f);
      CasParam g = cas_param(&s, 0.0f, 3);
      cas_hit(&g, pt, 0.1f * nearP, decay);
      cas_win_gain(&s, w, &g);
      cas_win_mix(&s, w, 1.0f);
    }
  }

  for (u32 i = 0; i < s.n; i++)
    s.out[i] *= 0.46f; // the voice trim: the loudest thing in the game
  if (dist > 0.0f)
    cas_apply_filter(s.out, s.n, cas_air_cutoff(dist), 0.0f);
  return s;
}

// a supersonic round passing `miss` metres from the ear
hz_internal CasSyn cas_voice_whizz(Allocator *al, u64 seed, f32 miss) {
  const f32 dur = 0.055f + miss * 0.012f;
  CasSyn s = cas_syn_begin(al, seed, (u32)m_ceilf((dur * 2.0f + 0.1f) * (f32)CAS_SR));
  const f32 level = cas_clamp(1.1f - miss / 6.0f, 0.1f, 1.0f);
  const f32 rate = cas_range(&s, 0.9f, 1.2f);
  CasWin w = cas_win(&s, 0.0f, cas_src_len(dur * 2.0f, rate, 0.004f + dur + 0.002f));
  cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
  cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, 0.0f, cas_range(&s, 3600.0f, 5200.0f), cas_range(&s, 900.0f, 1500.0f), dur, 3.2f);
  CasParam g = cas_param(&s, 0.0f, 5);
  cas_ad(&g, 0.0f, 1.5f * level, 0.004f, dur);
  cas_win_gain(&s, w, &g);
  cas_win_mix(&s, w, 1.0f);
  CasWin w2 = cas_win(&s, 0.0f, 0.006f + 0.002f);
  cas_win_noise(&s, w2, CAS_NOISE_WHITE, 1.2f);
  cas_win_svf_c(w2, PG_SVF_HIGHPASS, 4000.0f, 0.7f);
  CasParam g2 = cas_param(&s, 0.0f, 3);
  cas_hit(&g2, 0.0f, 0.85f * level, 0.006f);
  cas_win_gain(&s, w2, &g2);
  cas_win_mix(&s, w2, 1.0f);
  for (u32 i = 0; i < s.n; i++)
    s.out[i] *= 3.2f; // the voice trim
  return s;
}

// the hammer click on an empty chamber
hz_internal CasSyn cas_voice_dryfire(Allocator *al, u64 seed) {
  CasSyn s = cas_syn_begin(al, seed, (u32)(0.2f * (f32)CAS_SR));
  cas_struck4(&s, 0.0f, 0.0025f, 3, (CasPartial){2600.0f * cas_range(&s, 0.95f, 1.05f), 24.0f, 1.2f, 0.035f},
              (CasPartial){4700.0f, 16.0f, 0.66f, 0.02f}, (CasPartial){860.0f, 10.0f, 0.5f, 0.05f}, (CasPartial){0});
  return s;
}
