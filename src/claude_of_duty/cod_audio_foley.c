// Claude of Duty audio, foley voices: impacts and footsteps per surface, casings, reload parts, explosions, body falls, UI.

#define CAS_SURFACES COD_SURFACE_COUNT

typedef enum CasGait {
  CAS_GAIT_WALK = 0,
  CAS_GAIT_RUN,
  CAS_GAIT_SPRINT,
  CAS_GAIT_CROUCH,
  CAS_GAIT_LAND,
  CAS_GAIT_COUNT,
} CasGait;

// impacts, one entry per CodSurface: the mass thump, the material texture, the ring, the dust and the debris
typedef struct CasImpact {
  f32 bright, body_f, body_decay;
  u32 ring_n;
  CasPartial r0, r1, r2, r3;
  u32 tex_kind;
  f32 tex_f, tex_q, tex_decay, tex_level;
  b32 tex_rise;
  f32 dust_f, dust_decay, dust_level;
  u32 grains;
  f32 wet;
  b32 bubbles, squelch;
} CasImpact;

static const CasImpact CAS_IMPACT[CAS_SURFACES] = {
    // concrete
    {0.85f, 180, 0.05f, 0, {0}, {0}, {0}, {0}, CAS_NOISE_WHITE, 2600, 0.9f, 0.075f, 0.75f, false, 1200, 0.3f, 0.16f, 5, 0.4f, false, false},
    // metal
    {1.0f, 150, 0.035f, 4, {1750, 34, 0.42f, 0.28f}, {3120, 26, 0.3f, 0.17f}, {5400, 18, 0.18f, 0.09f}, {8100, 12, 0.09f, 0.05f},
     CAS_NOISE_WHITE, 5200, 1.2f, 0.03f, 0.5f, false, 0, 0, 0, 3, 0.5f, false, false},
    // wood
    {0.6f, 320, 0.055f, 3, {420, 14, 0.35f, 0.11f}, {780, 11, 0.2f, 0.07f}, {1520, 8, 0.1f, 0.04f}, {0},
     CAS_NOISE_WHITE, 1500, 1.0f, 0.045f, 0.45f, false, 0, 0, 0, 5, 0.32f, false, false},
    // dirt
    {0.25f, 120, 0.07f, 0, {0}, {0}, {0}, {0}, CAS_NOISE_BROWN, 700, 0.7f, 0.09f, 0.7f, false, 600, 0.34f, 0.2f, 4, 0.2f, false, false},
    // sand
    {0.18f, 105, 0.055f, 0, {0}, {0}, {0}, {0}, CAS_NOISE_WHITE, 1500, 0.5f, 0.13f, 0.5f, false, 1000, 0.4f, 0.24f, 3, 0.16f, false, false},
    // glass
    {1.0f, 500, 0.02f, 4, {3400, 40, 0.34f, 0.13f}, {5300, 34, 0.26f, 0.1f}, {7900, 26, 0.2f, 0.07f}, {11200, 18, 0.12f, 0.05f},
     CAS_NOISE_CRACKLE, 6000, 0.9f, 0.28f, 0.6f, false, 0, 0, 0, 11, 0.46f, false, false},
    // water
    {0.3f, 260, 0.03f, 0, {0}, {0}, {0}, {0}, CAS_NOISE_WHITE, 1800, 0.8f, 0.14f, 0.75f, true, 0, 0, 0, 4, 0.3f, true, false},
    // foliage
    {0.25f, 380, 0.02f, 0, {0}, {0}, {0}, {0}, CAS_NOISE_CRACKLE, 2600, 0.8f, 0.16f, 0.6f, false, 0, 0, 0, 7, 0.22f, false, false},
    // fabric
    {0.2f, 150, 0.045f, 0, {0}, {0}, {0}, {0}, CAS_NOISE_WHITE, 900, 0.6f, 0.06f, 0.4f, false, 700, 0.2f, 0.1f, 2, 0.18f, false, false},
    // flesh
    {0.35f, 128, 0.06f, 0, {0}, {0}, {0}, {0}, CAS_NOISE_WHITE, 620, 1.4f, 0.055f, 0.62f, false, 0, 0, 0, 3, 0.24f, false, true},
    // rubber
    {0.3f, 190, 0.04f, 1, {260, 9, 0.2f, 0.06f}, {0}, {0}, {0}, CAS_NOISE_WHITE, 1100, 0.9f, 0.03f, 0.3f, false, 0, 0, 0, 1, 0.2f, false, false},
    // plaster
    {0.7f, 220, 0.035f, 0, {0}, {0}, {0}, {0}, CAS_NOISE_WHITE, 1900, 0.8f, 0.05f, 0.6f, false, 900, 0.42f, 0.26f, 6, 0.42f, false, false},
};

hz_internal CasPartial cas_impact_ring(const CasImpact *s, u32 i) {
  return i == 0 ? s->r0 : (i == 1 ? s->r1 : (i == 2 ? s->r2 : s->r3));
}

// the energy rides the voice gain
hz_internal CasSyn cas_voice_impact(Allocator *al, u64 seed, u32 surface) {
  const CasImpact *S = &CAS_IMPACT[surface < CAS_SURFACES ? surface : 0];
  CasSyn s = cas_syn_begin(al, seed, (u32)(0.95f * (f32)CAS_SR));
  const f32 e = 1.0f;
  const f32 jit = cas_semis(cas_range(&s, -2.5f, 2.5f));
  const f32 t0 = 0.0f;

  if (S->bright > 0.05f) {
    const f32 rate = cas_range(&s, 0.9f, 1.35f);
    const f32 decay = cas_range(&s, 0.003f, 0.008f);
    CasWin w = cas_win(&s, t0, cas_src_len(0.04f, rate, decay + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
    cas_win_svf_c(w, PG_SVF_HIGHPASS, 3000.0f * jit, 0.7f);
    CasParam g = cas_param(&s, 0.0f, 3);
    cas_hit(&g, t0, 0.55f * S->bright * e, decay);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }
  {
    const f32 decay = S->body_decay * cas_range(&s, 0.85f, 1.2f);
    CasWin w = cas_win(&s, t0, 0.0015f + decay + 0.002f);
    cas_win_osc_sweep(&s, w, PG_OSC_SINE, t0, S->body_f * jit * 1.6f, S->body_f * jit * 0.7f, S->body_decay * 1.5f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 0.5f * e, 0.0015f, decay);
    cas_win_gain(&s, w, &g);
    cas_win_shape(&s, w, cas_curve(&s, 2.5f, 0.4f), 2);
    cas_win_mix(&s, w, 1.0f);
  }
  {
    const f32 rate = cas_range(&s, 0.8f, 1.3f);
    const f32 decay = S->tex_decay * cas_range(&s, 0.85f, 1.25f);
    const f32 attack = S->tex_rise ? 0.008f : 0.0015f;
    CasWin w = cas_win(&s, t0, cas_src_len(S->tex_decay * 3.0f + 0.05f, rate, attack + decay + 0.002f));
    cas_win_noise(&s, w, S->tex_kind, rate);
    if (S->tex_rise)
      cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, t0, S->tex_f * 0.4f, S->tex_f * 2.2f, S->tex_decay, S->tex_q);
    else
      cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, t0, S->tex_f * 1.5f * jit, S->tex_f * 0.6f * jit, S->tex_decay * 1.6f, S->tex_q);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, S->tex_level * e, attack, decay);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }
  if (S->ring_n) {
    CasPartial *p = ALLOC_ARRAY(s.al, CasPartial, 4);
    for (u32 i = 0; i < S->ring_n; i++) {
      const CasPartial r = cas_impact_ring(S, i);
      p[i] = (CasPartial){r.f * cas_semis(cas_range(&s, -3.0f, 3.0f)), r.q * cas_range(&s, 0.8f, 1.25f), r.g * e,
                          r.decay * cas_range(&s, 0.75f, 1.3f)};
    }
    cas_struck(&s, t0, p, S->ring_n, 0.0035f);
  }
  if (S->dust_level > 0.0f) {
    const f32 rate = cas_range(&s, 0.7f, 1.1f);
    const f32 decay = S->dust_decay * cas_range(&s, 0.8f, 1.3f);
    CasWin w = cas_win(&s, t0, cas_src_len(S->dust_decay * 2.0f + 0.05f, rate, 0.02f + decay + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
    cas_win_svf_sweep(&s, w, PG_SVF_LOWPASS, t0, S->dust_f * 1.4f, S->dust_f * 0.5f, S->dust_decay, 0.8f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, S->dust_level * e, 0.02f, decay);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }
  const u32 grains = (u32)m_roundf((f32)S->grains * cas_clamp(e, 0.3f, 1.4f));
  for (u32 i = 0; i < grains; i++) {
    const f32 gt = t0 + cas_range(&s, 0.015f, 0.06f) + (f32)i * cas_range(&s, 0.01f, 0.055f);
    cas_struck1(&s, gt, 0.0018f,
                (CasPartial){cas_range(&s, 1800.0f, 9000.0f), cas_range(&s, 12.0f, 30.0f), cas_range(&s, 0.02f, 0.05f) * e,
                             cas_range(&s, 0.01f, 0.05f)});
  }
  if (S->bubbles) {
    for (u32 i = 0; i < 4; i++) {
      const f32 bt = t0 + cas_range(&s, 0.02f, 0.18f);
      const f32 f0 = cas_range(&s, 400.0f, 1400.0f); // the oscillator's constructor frequency, replaced by the sweep
      (void)f0;
      CasWin w = cas_win(&s, bt, 0.05f + 0.002f);
      cas_win_osc_sweep(&s, w, PG_OSC_SINE, bt, cas_range(&s, 350.0f, 700.0f), cas_range(&s, 900.0f, 2200.0f), 0.05f);
      CasParam g = cas_param(&s, 0.0f, 3);
      cas_hit(&g, bt, cas_range(&s, 0.04f, 0.1f) * e, 0.05f);
      cas_win_gain(&s, w, &g);
      cas_win_mix(&s, w, 1.0f);
    }
  }
  if (S->squelch) {
    const f32 rate = cas_range(&s, 0.7f, 1.1f);
    CasWin w = cas_win(&s, t0, cas_src_len(0.25f, rate, 0.004f + 0.006f + 0.1f + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_PINK, rate);
    cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, t0, 260.0f, 900.0f, 0.09f, 2.2f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0 + 0.004f, 0.4f * e, 0.006f, 0.1f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }
  for (u32 i = 0; i < s.n; i++)
    s.out[i] *= 0.22f; // the voice trim
  return s;
}

/* ------------------------------------------------------------ footsteps */

typedef struct CasStep {
  f32 body_f, body_decay;
  u32 tex_kind;
  f32 tex_f, tex_q, tex_decay, tex_level, scuff;
  u32 grit;
  u32 ring_n;
  CasPartial r0, r1, r2;
  b32 splash;
} CasStep;

// footsteps, one entry per CodSurface
static const CasStep CAS_STEP[CAS_SURFACES] = {
    {92, 0.055f, CAS_NOISE_WHITE, 2100, 0.7f, 0.045f, 0.5f, 0.35f, 4, 0, {0}, {0}, {0}, false},                                   // concrete
    {120, 0.05f, CAS_NOISE_WHITE, 3200, 1.0f, 0.04f, 0.5f, 0.3f, 2, 3, {620, 16, 0.24f, 0.16f}, {1480, 20, 0.16f, 0.11f}, {2900, 14, 0.08f, 0.06f}, false}, // metal
    {110, 0.06f, CAS_NOISE_WHITE, 1300, 0.8f, 0.04f, 0.4f, 0.28f, 2, 2, {260, 12, 0.26f, 0.09f}, {540, 9, 0.14f, 0.05f}, {0}, false}, // wood
    {78, 0.07f, CAS_NOISE_BROWN, 620, 0.6f, 0.075f, 0.62f, 0.45f, 6, 0, {0}, {0}, {0}, false},                                    // dirt
    {70, 0.06f, CAS_NOISE_WHITE, 1500, 0.45f, 0.14f, 0.6f, 0.7f, 3, 0, {0}, {0}, {0}, false},                                     // sand
    {96, 0.04f, CAS_NOISE_CRACKLE, 5200, 0.8f, 0.2f, 0.6f, 0.3f, 9, 0, {0}, {0}, {0}, false},                                     // glass
    {88, 0.045f, CAS_NOISE_WHITE, 1600, 0.7f, 0.17f, 0.8f, 0.5f, 3, 0, {0}, {0}, {0}, true},                                      // water
    {84, 0.05f, CAS_NOISE_CRACKLE, 2400, 0.7f, 0.18f, 0.7f, 0.5f, 6, 0, {0}, {0}, {0}, false},                                    // foliage
    {82, 0.05f, CAS_NOISE_WHITE, 800, 0.6f, 0.05f, 0.3f, 0.35f, 0, 0, {0}, {0}, {0}, false},                                      // fabric
    {86, 0.055f, CAS_NOISE_WHITE, 520, 1.2f, 0.05f, 0.35f, 0.2f, 0, 0, {0}, {0}, {0}, false},                                     // flesh
    {96, 0.04f, CAS_NOISE_WHITE, 1000, 0.8f, 0.03f, 0.28f, 0.2f, 0, 0, {0}, {0}, {0}, false},                                     // rubber
    {100, 0.05f, CAS_NOISE_WHITE, 1800, 0.7f, 0.05f, 0.45f, 0.3f, 4, 0, {0}, {0}, {0}, false},                                    // plaster
};

hz_internal f32 cas_gait_weight(u32 gait) {
  switch (gait) {
  case CAS_GAIT_SPRINT: return 1.25f;
  case CAS_GAIT_RUN: return 1.0f;
  case CAS_GAIT_LAND: return 1.7f;
  case CAS_GAIT_CROUCH: return 0.42f;
  default: return 0.62f;
  }
}

// heel and toe, the scuff, grit, the splash and the gear
hz_internal CasSyn cas_voice_step(Allocator *al, u64 seed, u32 surface, u32 gait) {
  const CasStep *S = &CAS_STEP[surface < CAS_SURFACES ? surface : 0];
  CasSyn s = cas_syn_begin(al, seed, (u32)(0.9f * (f32)CAS_SR));
  const f32 lvl = cas_gait_weight(gait);
  const f32 jit = cas_semis(cas_range(&s, -3.0f, 3.0f));
  const f32 t0 = 0.0f;
  const u32 contacts = gait == CAS_GAIT_LAND ? 1u : 2u;
  for (u32 c = 0; c < contacts; c++) {
    const f32 ct = t0 + (c == 0 ? 0.0f : cas_range(&s, 0.012f, 0.032f));
    const f32 cl = c == 0 ? 1.0f : cas_range(&s, 0.35f, 0.6f);
    {
      const f32 decay = S->body_decay * cas_range(&s, 0.85f, 1.2f);
      CasWin w = cas_win(&s, ct, 0.0025f + decay + 0.002f);
      cas_win_osc_sweep(&s, w, PG_OSC_SINE, ct, S->body_f * jit * 1.7f, S->body_f * jit * 0.75f, S->body_decay * 1.4f);
      CasParam g = cas_param(&s, 0.0f, 5);
      cas_ad(&g, ct, 0.42f * lvl * cl, 0.0025f, decay);
      cas_win_gain(&s, w, &g);
      cas_win_shape(&s, w, cas_curve(&s, 1.8f, 0.5f), 2);
      cas_win_mix(&s, w, 1.0f);
    }
    {
      const f32 rate = cas_range(&s, 0.8f, 1.25f);
      const f32 decay = S->tex_decay * cas_range(&s, 0.8f, 1.3f);
      CasWin w = cas_win(&s, ct, cas_src_len(S->tex_decay * 3.0f + 0.05f, rate, 0.002f + decay + 0.002f));
      cas_win_noise(&s, w, S->tex_kind, rate);
      cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, ct, S->tex_f * 1.4f * jit, S->tex_f * 0.55f * jit, S->tex_decay * 2.0f, S->tex_q);
      CasParam g = cas_param(&s, 0.0f, 5);
      cas_ad(&g, ct, S->tex_level * lvl * cl, 0.002f, decay);
      cas_win_gain(&s, w, &g);
      cas_win_mix(&s, w, 1.0f);
    }
    if (S->ring_n && c == 0) {
      CasPartial *p = ALLOC_ARRAY(s.al, CasPartial, 3);
      for (u32 i = 0; i < S->ring_n; i++) {
        const CasPartial r = i == 0 ? S->r0 : (i == 1 ? S->r1 : S->r2);
        p[i] = (CasPartial){r.f * cas_semis(cas_range(&s, -2.0f, 2.0f)), r.q * cas_range(&s, 0.85f, 1.2f), r.g * lvl,
                            r.decay * cas_range(&s, 0.8f, 1.25f)};
      }
      cas_struck(&s, ct, p, S->ring_n, 0.003f);
    }
  }
  if (S->scuff > 0.05f) {
    const f32 st = t0 + cas_range(&s, 0.01f, 0.04f);
    const f32 dur = (gait == CAS_GAIT_SPRINT ? 0.13f : (gait == CAS_GAIT_RUN ? 0.1f : 0.07f)) * cas_range(&s, 0.8f, 1.3f);
    const f32 rate = cas_range(&s, 0.85f, 1.2f);
    CasWin w = cas_win(&s, st, cas_src_len(dur * 2.0f, rate, 0.012f + dur + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
    const f32 bp = cas_range(&s, 2200.0f, 4200.0f);
    (void)bp;
    cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, st, cas_range(&s, 2800.0f, 4600.0f), cas_range(&s, 1200.0f, 2000.0f), dur, 0.8f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, st, S->scuff * lvl * 0.5f, 0.012f, dur);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }
  for (u32 i = 0; i < S->grit; i++) {
    if (cas_unit(&s) > 0.55f)
      continue;
    const f32 gt = t0 + cas_range(&s, 0.004f, 0.09f);
    cas_struck1(&s, gt, 0.0015f,
                (CasPartial){cas_range(&s, 2400.0f, 9000.0f), cas_range(&s, 10.0f, 26.0f), cas_range(&s, 0.015f, 0.05f) * lvl,
                             cas_range(&s, 0.008f, 0.03f)});
  }
  if (S->splash) {
    const f32 rate = cas_range(&s, 0.9f, 1.2f);
    CasWin w = cas_win(&s, t0, cas_src_len(0.4f, rate, 0.006f + 0.01f + 0.2f + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
    cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, t0, 700.0f, 3400.0f, 0.16f, 0.7f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0 + 0.006f, 0.45f * lvl, 0.01f, 0.2f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }
  const f32 gear = gait == CAS_GAIT_SPRINT ? 1.0f : (gait == CAS_GAIT_RUN ? 0.7f : (gait == CAS_GAIT_LAND ? 0.9f : 0.25f));
  if (gear > 0.05f) {
    const u32 n = 1u + (cod_rng_u32(&s.rng) % 3u);
    for (u32 i = 0; i < n; i++) {
      const f32 gt = t0 + cas_range(&s, 0.005f, 0.11f);
      cas_struck4(&s, gt, 0.002f, 2,
                  (CasPartial){cas_range(&s, 1600.0f, 4200.0f), cas_range(&s, 18.0f, 40.0f), cas_range(&s, 0.03f, 0.1f) * gear * lvl,
                               cas_range(&s, 0.03f, 0.12f)},
                  (CasPartial){cas_range(&s, 4200.0f, 8000.0f), cas_range(&s, 12.0f, 26.0f), cas_range(&s, 0.01f, 0.04f) * gear * lvl,
                               cas_range(&s, 0.01f, 0.05f)},
                  (CasPartial){0}, (CasPartial){0});
    }
    const f32 rate = cas_range(&s, 0.7f, 1.1f);
    CasWin w = cas_win(&s, t0, cas_src_len(0.3f, rate, 0.02f + 0.13f + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
    cas_win_svf_c(w, PG_SVF_BANDPASS, cas_range(&s, 1400.0f, 2600.0f), 0.6f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 0.09f * gear * lvl, 0.02f, 0.13f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }
  for (u32 i = 0; i < s.n; i++)
    s.out[i] *= 0.32f; // the voice trim
  return s;
}

/* ------------------------------------------------------------ cloth, casings, reloads */

hz_internal CasSyn cas_voice_cloth(Allocator *al, u64 seed) {
  CasSyn s = cas_syn_begin(al, seed, (u32)(0.8f * (f32)CAS_SR));
  const f32 dur = cas_range(&s, 0.13f, 0.26f);
  const f32 rate = cas_range(&s, 0.7f, 1.15f);
  CasWin w = cas_win(&s, 0.0f, cas_src_len(dur * 2.0f, rate, 0.03f + dur + 0.002f));
  cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
  const f32 bp0 = cas_range(&s, 1300.0f, 2400.0f);
  (void)bp0;
  cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, 0.0f, cas_range(&s, 1000.0f, 1600.0f), cas_range(&s, 2200.0f, 3400.0f), dur, 0.55f);
  CasParam g = cas_param(&s, 0.0f, 5);
  cas_ad(&g, 0.0f, 0.3f, 0.03f, dur);
  cas_win_gain(&s, w, &g);
  cas_win_mix(&s, w, 1.0f);
  if (cas_unit(&s) < 0.6f)
    cas_struck1(&s, cas_range(&s, 0.02f, 0.1f), 0.002f,
                (CasPartial){cas_range(&s, 2200.0f, 5200.0f), 26.0f, 0.05f, cas_range(&s, 0.03f, 0.09f)});
  return s;
}

hz_internal b32 cas_surface_hard(u32 surface) {
  return surface == COD_SURFACE_METAL || surface == COD_SURFACE_CONCRETE || surface == COD_SURFACE_GLASS || surface == COD_SURFACE_PLASTER;
}

hz_internal b32 cas_surface_soft(u32 surface) {
  return surface == COD_SURFACE_DIRT || surface == COD_SURFACE_SAND || surface == COD_SURFACE_FOLIAGE || surface == COD_SURFACE_FABRIC;
}

// bounces with shortening gaps, then a roll
hz_internal CasSyn cas_voice_shell(Allocator *al, u64 seed, u32 surface) {
  CasSyn s = cas_syn_begin(al, seed, (u32)(1.6f * (f32)CAS_SR));
  const b32 hard = cas_surface_hard(surface), soft = cas_surface_soft(surface);
  const f32 base = cas_range(&s, 2650.0f, 4200.0f);
  f32 t = 0.0f;
  f32 amp = (soft ? 0.35f : 1.0f) * cas_range(&s, 0.8f, 1.1f);
  const u32 bounces = soft ? 1u : 2u + (cod_rng_u32(&s.rng) % 3u);
  for (u32 i = 0; i < bounces; i++) {
    const f32 detune = cas_semis(cas_range(&s, -4.0f, 4.0f));
    const f32 k = hard ? 1.0f : 0.5f;
    cas_struck4(&s, t, 0.0015f, 4, (CasPartial){base * detune, cas_range(&s, 30.0f, 60.0f), 0.95f * amp, cas_range(&s, 0.05f, 0.13f) * k},
                (CasPartial){base * detune * 1.87f, cas_range(&s, 24.0f, 44.0f), 0.58f * amp, cas_range(&s, 0.03f, 0.08f)},
                (CasPartial){base * detune * 3.1f, cas_range(&s, 16.0f, 30.0f), 0.3f * amp, cas_range(&s, 0.015f, 0.04f)},
                (CasPartial){base * detune * 0.42f, 12.0f, 0.2f * amp, 0.03f});
    t += cas_range(&s, 0.045f, 0.13f) * (i == 0 ? 1.0f : 0.6f);
    amp *= cas_range(&s, 0.38f, 0.62f);
    if (amp < 0.03f)
      break;
  }
  if (hard && cas_unit(&s) < 0.55f) {
    const u32 rolls = 3u + (cod_rng_u32(&s.rng) % 5u);
    for (u32 i = 0; i < rolls; i++) {
      const f32 rt = t + (f32)i * cas_range(&s, 0.018f, 0.05f);
      cas_struck1(&s, rt, 0.0012f,
                  (CasPartial){base * cas_semis(cas_range(&s, -5.0f, 5.0f)), cas_range(&s, 20.0f, 44.0f), cas_range(&s, 0.03f, 0.09f),
                               cas_range(&s, 0.01f, 0.03f)});
    }
  }
  return s;
}

// start, magout, magin, end (CodReloadPhase order); heavy = 1
hz_internal CasSyn cas_voice_reload(Allocator *al, u64 seed, u32 phase) {
  CasSyn s = cas_syn_begin(al, seed, (u32)(0.9f * (f32)CAS_SR));
  const f32 heavy = 1.0f;
  const f32 t0 = 0.0f;
  // a band of rustling noise
#define CAS_RUSTLE(T, DUR, LEVEL, F)                                                                              \
  do {                                                                                                          \
    const f32 rate_ = cas_range(&s, 0.8f, 1.2f);                                                                  \
    CasWin w_ = cas_win(&s, (T), cas_src_len((DUR) * 2.0f + 0.05f, rate_, 0.02f + (DUR) + 0.002f));              \
    cas_win_noise(&s, w_, CAS_NOISE_WHITE, rate_);                                                                \
    cas_win_svf_c(w_, PG_SVF_BANDPASS, (F), 0.6f);                                                                \
    CasParam g_ = cas_param(&s, 0.0f, 5);                                                                         \
    cas_ad(&g_, (T), (LEVEL), 0.02f, (DUR));                                                                      \
    cas_win_gain(&s, w_, &g_);                                                                                    \
    cas_win_mix(&s, w_, 1.0f);                                                                                    \
  } while (0)
  switch (phase) {
  case COD_RELOAD_START:
    CAS_RUSTLE(t0, 0.18f, 0.2f, cas_range(&s, 1400.0f, 2200.0f));
    cas_struck4(&s, t0 + cas_range(&s, 0.04f, 0.08f), 0.0025f, 3,
                (CasPartial){2450.0f * cas_semis(cas_range(&s, -2.0f, 2.0f)), 30.0f, 0.55f * heavy, 0.03f},
                (CasPartial){5100.0f, 18.0f, 0.25f, 0.016f}, (CasPartial){780.0f, 10.0f, 0.3f * heavy, 0.045f}, (CasPartial){0});
    break;
  case COD_RELOAD_MAG_OUT: {
    cas_struck4(&s, t0, 0.0025f, 2, (CasPartial){1650.0f * cas_semis(cas_range(&s, -2.0f, 2.0f)), 24.0f, 0.65f * heavy, 0.05f},
                (CasPartial){3400.0f, 16.0f, 0.35f, 0.025f}, (CasPartial){0}, (CasPartial){0});
    const f32 st = t0 + 0.03f;
    const f32 rate = cas_range(&s, 0.9f, 1.3f);
    CasWin w = cas_win(&s, st, cas_src_len(0.3f, rate, 0.01f + 0.12f + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
    cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, st, 4200.0f, 1600.0f, 0.12f, 1.1f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, st, 0.2f, 0.01f, 0.12f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
    const f32 dt = t0 + cas_range(&s, 0.16f, 0.3f);
    cas_struck4(&s, dt, 0.004f, 3, (CasPartial){480.0f * cas_semis(cas_range(&s, -3.0f, 3.0f)), 9.0f, 0.2f, 0.05f},
                (CasPartial){1180.0f, 7.0f, 0.11f, 0.03f}, (CasPartial){2600.0f, 5.0f, 0.05f, 0.015f}, (CasPartial){0});
    break;
  }
  case COD_RELOAD_MAG_IN: {
    CAS_RUSTLE(t0, 0.12f, 0.16f, cas_range(&s, 1200.0f, 2000.0f));
    const f32 it = t0 + cas_range(&s, 0.05f, 0.1f);
    CasWin w = cas_win(&s, it, 0.002f + 0.055f + 0.002f);
    cas_win_osc_sweep(&s, w, PG_OSC_SINE, it, 230.0f * heavy, 110.0f * heavy, 0.06f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, it, 0.4f * heavy, 0.002f, 0.055f);
    cas_win_gain(&s, w, &g);
    cas_win_shape(&s, w, cas_curve(&s, 3.0f, 0.5f), 2);
    cas_win_mix(&s, w, 1.0f);
    cas_struck4(&s, it, 0.003f, 3, (CasPartial){1250.0f * cas_semis(cas_range(&s, -2.0f, 2.0f)), 20.0f, 0.3f * heavy, 0.06f},
                (CasPartial){2800.0f, 26.0f, 0.18f, 0.03f}, (CasPartial){6200.0f, 14.0f, 0.07f, 0.012f}, (CasPartial){0});
    cas_struck4(&s, it + cas_range(&s, 0.02f, 0.05f), 0.0015f, 2, (CasPartial){3600.0f, 34.0f, 0.16f, 0.02f},
                (CasPartial){7400.0f, 20.0f, 0.07f, 0.01f}, (CasPartial){0}, (CasPartial){0});
    break;
  }
  default: {
    const f32 st = t0;
    const f32 rate = cas_range(&s, 0.9f, 1.25f);
    CasWin w = cas_win(&s, st, cas_src_len(0.2f, rate, 0.008f + 0.07f + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
    cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, st, 1800.0f, 4200.0f, 0.07f, 1.6f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, st, 0.24f, 0.008f, 0.07f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
    cas_struck4(&s, st + 0.06f, 0.0025f, 2, (CasPartial){1450.0f * cas_semis(cas_range(&s, -2.0f, 2.0f)), 22.0f, 0.3f * heavy, 0.05f},
                (CasPartial){3100.0f, 18.0f, 0.16f, 0.022f}, (CasPartial){0}, (CasPartial){0});
    cas_struck4(&s, st + 0.065f, 0.002f, 2, (CasPartial){4900.0f * cas_semis(cas_range(&s, -3.0f, 3.0f)), 46.0f, 0.09f, 0.16f},
                (CasPartial){7200.0f, 38.0f, 0.05f, 0.1f}, (CasPartial){0}, (CasPartial){0});
    const f32 bt = st + cas_range(&s, 0.1f, 0.15f);
    CasWin b = cas_win(&s, bt, 0.0015f + 0.05f + 0.002f);
    cas_win_osc_sweep(&s, b, PG_OSC_SINE, bt, 200.0f * heavy, 90.0f * heavy, 0.05f);
    CasParam bg = cas_param(&s, 0.0f, 5);
    cas_ad(&bg, bt, 0.38f * heavy, 0.0015f, 0.05f);
    cas_win_gain(&s, b, &bg);
    cas_win_mix(&s, b, 1.0f);
    cas_struck4(&s, bt, 0.0035f, 3, (CasPartial){1750.0f, 20.0f, 0.34f * heavy, 0.05f}, (CasPartial){3900.0f, 15.0f, 0.15f, 0.02f},
                (CasPartial){8200.0f, 10.0f, 0.05f, 0.008f}, (CasPartial){0});
    break;
  }
  }
#undef CAS_RUSTLE
  const f32 trim = phase == COD_RELOAD_START ? 3.2f : (phase == COD_RELOAD_MAG_OUT ? 3.0f : (phase == COD_RELOAD_MAG_IN ? 1.0f : 1.5f));
  const f32 gain = 0.42f * trim;
  for (u32 i = 0; i < s.n; i++)
    s.out[i] *= gain;
  return s;
}

/* ------------------------------------------------------------ explosions, body falls, heartbeat */

// the radii the explosion sets are rendered for (a grenade, and the distant booms' range)
#define CAS_EXPLOSION_SIZES 3
static const f32 CAS_EXPLOSION_RADIUS[CAS_EXPLOSION_SIZES] = {6.5f, 11.0f, 16.0f};

hz_internal f32 cas_explosion_seconds(f32 radius, f32 dist) {
  const f32 size = cas_clamp(radius / 6.0f, 0.5f, 2.4f);
  const f32 far = 1.0f - cas_clamp(1.0f - dist / 70.0f, 0.0f, 1.0f);
  const f32 sub = (0.55f + size * 0.35f) * 1.15f * 1.6f;
  const f32 blast = (0.45f + size * 0.5f) * (1.0f + far * 1.8f) * 1.4f + 0.1f;
  const f32 dust = 0.05f + 0.12f + (1.0f + size * 0.8f) + 0.01f;
  return m_maxf(m_maxf(sub, blast), m_maxf(dust, 1.2f)) + 0.1f;
}

// detonation transient, sub sweep, blast body, shrapnel grains, dust
hz_internal CasSyn cas_voice_explosion(Allocator *al, u64 seed, f32 radius, f32 dist) {
  CasSyn s = cas_syn_begin(al, seed, (u32)m_ceilf(cas_explosion_seconds(radius, dist) * (f32)CAS_SR));
  const f32 size = cas_clamp(radius / 6.0f, 0.5f, 2.4f);
  const f32 near_v = cas_clamp(1.0f - dist / 70.0f, 0.0f, 1.0f);
  const f32 far = 1.0f - near_v;
  const f32 lvl = size;
  const f32 t0 = 0.0f;
  if (near_v > 0.05f) {
    const f32 rate = cas_range(&s, 0.9f, 1.2f);
    CasWin w = cas_win(&s, t0, cas_src_len(0.1f, rate, 0.02f + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
    cas_win_svf_c(w, PG_SVF_HIGHPASS, 1800.0f, 0.6f);
    cas_win_shape(&s, w, cas_curve(&s, 14.0f, 0.7f), 4);
    CasParam g = cas_param(&s, 0.0f, 3);
    cas_hit(&g, t0, 0.85f * near_v * lvl, 0.02f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }
  {
    const f32 sub = (0.55f + size * 0.35f) * cas_range(&s, 0.9f, 1.15f);
    const f32 attack = 0.008f + far * 0.05f;
    const f32 env_end = attack + sub + 0.002f;
    CasWin w1 = cas_win(&s, t0, env_end);
    cas_win_osc_sweep(&s, w1, PG_OSC_SINE, t0, 130.0f * size, 26.0f, sub);
    CasWin w2 = cas_win(&s, t0, env_end);
    cas_win_osc_sweep(&s, w2, PG_OSC_TRI, t0, 74.0f * size, 21.0f, sub * 1.2f);
    for (u32 i = 0; i < w1.len; i++)
      w1.x[i] += w2.x[i];
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 1.0f * lvl * (0.55f + near_v * 0.6f), attack, sub);
    cas_win_gain(&s, w1, &g);
    cas_win_shape(&s, w1, cas_curve(&s, 4.0f, 0.6f), 2);
    cas_win_svf_c(w1, PG_SVF_LOWPASS, 220.0f, 0.9f);
    cas_win_mix(&s, w1, 1.0f);
  }
  {
    const f32 dur = (0.45f + size * 0.5f) * (1.0f + far * 1.8f);
    const f32 rate = cas_range(&s, 0.6f, 1.1f);
    const f32 attack = 0.01f + far * 0.06f;
    CasWin w = cas_win(&s, t0, cas_src_len(dur * 1.4f + 0.1f, rate, attack + dur + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_BROWN, rate);
    cas_win_svf_sweep(&s, w, PG_SVF_LOWPASS, t0, cas_lerp(7000.0f, 700.0f, far), cas_lerp(260.0f, 130.0f, far), dur, 0.8f);
    cas_win_shape(&s, w, cas_curve(&s, 6.0f, 0.5f), 2);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 0.8f * lvl, attack, dur);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }
  const u32 grains = (u32)m_roundf(cas_lerp(26.0f, 4.0f, far) * size);
  for (u32 i = 0; i < grains; i++) {
    const f32 gt = t0 + cas_range(&s, 0.02f, 0.9f) * cas_range(&s, 0.3f, 1.0f);
    const CasPartial p = {cas_range(&s, 700.0f, 7000.0f), cas_range(&s, 8.0f, 32.0f), cas_range(&s, 0.02f, 0.09f) * near_v * lvl,
                          cas_range(&s, 0.01f, 0.09f)};
    if (p.g > 1e-5f)
      cas_struck1(&s, gt, 0.002f, p);
  }
  {
    const f32 dur = 1.0f + size * 0.8f;
    const f32 rate = cas_range(&s, 0.5f, 0.9f);
    CasWin w = cas_win(&s, t0 + 0.05f, cas_src_len(dur * 1.3f, rate, 0.12f + dur + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_PINK, rate);
    cas_win_svf_sweep(&s, w, PG_SVF_LOWPASS, t0 + 0.05f, 1600.0f, 320.0f, dur, 0.7f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0 + 0.05f, 0.2f * lvl * (0.4f + near_v * 0.6f), 0.12f, dur);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }
  for (u32 i = 0; i < s.n; i++)
    s.out[i] *= 0.42f; // the voice trim
  return s;
}

hz_internal CasSyn cas_voice_bodyfall(Allocator *al, u64 seed) {
  CasSyn s = cas_syn_begin(al, seed, (u32)(0.8f * (f32)CAS_SR));
  CasWin b = cas_win(&s, 0.0f, 0.004f + 0.13f + 0.002f);
  cas_win_osc_sweep(&s, b, PG_OSC_SINE, 0.0f, 96.0f, 44.0f, 0.12f);
  CasParam bg = cas_param(&s, 0.0f, 5);
  cas_ad(&bg, 0.0f, 0.6f, 0.004f, 0.13f);
  cas_win_gain(&s, b, &bg);
  cas_win_shape(&s, b, cas_curve(&s, 2.2f, 0.55f), 2);
  cas_win_mix(&s, b, 1.0f);
  const f32 rate = cas_range(&s, 0.7f, 1.1f);
  CasWin w = cas_win(&s, 0.0f, cas_src_len(0.4f, rate, 0.006f + 0.16f + 0.002f));
  cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
  cas_win_svf_c(w, PG_SVF_LOWPASS, 900.0f, 0.8f);
  CasParam g = cas_param(&s, 0.0f, 5);
  cas_ad(&g, 0.0f, 0.35f, 0.006f, 0.16f);
  cas_win_gain(&s, w, &g);
  cas_win_mix(&s, w, 1.0f);
  for (u32 i = 0; i < 5; i++)
    cas_struck1(&s, cas_range(&s, 0.005f, 0.26f), 0.002f,
                (CasPartial){cas_range(&s, 1500.0f, 5200.0f), cas_range(&s, 16.0f, 40.0f), cas_range(&s, 0.03f, 0.09f),
                             cas_range(&s, 0.02f, 0.1f)});
  for (u32 i = 0; i < s.n; i++)
    s.out[i] *= 0.4f; // the voice trim
  return s;
}

// lub and dub
hz_internal CasSyn cas_voice_heartbeat(Allocator *al, u64 seed) {
  CasSyn s = cas_syn_begin(al, seed, (u32)(0.7f * (f32)CAS_SR));
  for (u32 i = 0; i < 2; i++) {
    const f32 bt = (f32)i * 0.19f;
    CasWin w = cas_win(&s, bt, 0.008f + 0.11f + 0.002f);
    cas_win_osc_sweep(&s, w, PG_OSC_SINE, bt, 72.0f, 42.0f, 0.1f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, bt, i == 0 ? 0.5f : 0.33f, 0.008f, 0.11f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
  }
  for (u32 i = 0; i < s.n; i++)
    s.out[i] *= 0.5f; // the voice trim
  return s;
}

/* ------------------------------------------------------------ UI */

typedef enum CasUi {
  CAS_UI_HITMARKER = 0,
  CAS_UI_HEADSHOT,
  CAS_UI_KILL,
  CAS_UI_DAMAGE,
  CAS_UI_ARMOUR,
  CAS_UI_GRENADE_WARN,
  CAS_UI_REGEN,
  CAS_UI_LOWHEALTH,
  CAS_UI_CLICK,
  CAS_UI_COUNT,
} CasUi;

// short, dry and deliberately synthetic
hz_internal CasSyn cas_voice_ui(Allocator *al, u64 seed, u32 kind) {
  CasSyn s = cas_syn_begin(al, seed, (u32)(1.0f * (f32)CAS_SR));
  const f32 t0 = 0.0f;
  switch (kind) {
  case CAS_UI_HITMARKER: {
    CasWin w = cas_win(&s, t0, 0.022f + 0.002f);
    cas_win_osc_c(&s, w, PG_OSC_SQUARE, 2400.0f);
    CasParam g = cas_param(&s, 0.0f, 3);
    cas_hit(&g, t0, 0.55f, 0.022f);
    cas_win_gain(&s, w, &g);
    cas_win_svf_c(w, PG_SVF_LOWPASS, 5200.0f, 0.7f);
    cas_win_mix(&s, w, 1.0f);
    break;
  }
  case CAS_UI_HEADSHOT: {
    CasWin w = cas_win(&s, t0, 0.05f + 0.002f);
    cas_win_osc_c(&s, w, PG_OSC_SQUARE, 3200.0f);
    CasWin w2 = cas_win(&s, t0 + 0.03f, 0.05f + 0.002f - 0.03f);
    cas_win_osc_c(&s, w2, PG_OSC_SQUARE, 4800.0f);
    for (u32 i = 0; i < w2.len && w2.a + i - w.a < w.len; i++)
      w.x[w2.a + i - w.a] += w2.x[i];
    CasParam g = cas_param(&s, 0.0f, 3);
    cas_hit(&g, t0, 0.34f, 0.05f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
    break;
  }
  case CAS_UI_KILL: {
    for (u32 i = 0; i < 3; i++) {
      const f32 bt = t0 + (f32)i * 0.055f;
      CasWin w = cas_win(&s, bt, 0.004f + 0.09f + 0.002f);
      cas_win_osc_c(&s, w, PG_OSC_TRI, 900.0f * m_powf(1.5f, (f32)i));
      CasParam g = cas_param(&s, 0.0f, 5);
      cas_ad(&g, bt, 0.3f, 0.004f, 0.09f);
      cas_win_gain(&s, w, &g);
      cas_win_mix(&s, w, 1.0f);
    }
    break;
  }
  case CAS_UI_DAMAGE: {
    CasWin w = cas_win(&s, t0, 0.004f + 0.22f + 0.002f);
    cas_win_osc_c(&s, w, PG_OSC_SAW, 180.0f);
    CasWin w2 = cas_win(&s, t0, 0.004f + 0.22f + 0.002f);
    cas_win_osc_c(&s, w2, PG_OSC_SAW, 191.0f);
    for (u32 i = 0; i < w.len; i++)
      w.x[i] += w2.x[i];
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 0.42f, 0.004f, 0.22f);
    cas_win_gain(&s, w, &g);
    cas_win_svf_c(w, PG_SVF_LOWPASS, 1400.0f, 1.4f);
    cas_win_mix(&s, w, 1.0f);
    break;
  }
  case CAS_UI_ARMOUR:
    cas_struck4(&s, t0, 0.0015f, 2, (CasPartial){3900.0f, 30.0f, 0.09f, 0.045f}, (CasPartial){6400.0f, 22.0f, 0.05f, 0.025f},
                (CasPartial){0}, (CasPartial){0});
    break;
  case CAS_UI_GRENADE_WARN: {
    for (u32 i = 0; i < 3; i++) {
      const f32 bt = t0 + (f32)i * 0.14f;
      CasWin w = cas_win(&s, bt, 0.004f + 0.07f + 0.002f);
      cas_win_osc_c(&s, w, PG_OSC_SQUARE, 1150.0f * m_powf(1.19f, (f32)i));
      CasParam g = cas_param(&s, 0.0f, 5);
      cas_ad(&g, bt, 0.3f, 0.004f, 0.07f);
      cas_win_gain(&s, w, &g);
      cas_win_svf_c(w, PG_SVF_LOWPASS, 4200.0f, 0.8f);
      cas_win_mix(&s, w, 1.0f);
    }
    break;
  }
  case CAS_UI_REGEN: {
    CasWin w = cas_win(&s, t0, cas_src_len(0.9f, 0.9f, 0.15f + 0.45f + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_PINK, 0.9f);
    cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, t0, 500.0f, 1900.0f, 0.5f, 1.1f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 0.3f, 0.15f, 0.45f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
    CasWin o = cas_win(&s, t0, 0.14f + 0.4f + 0.002f);
    cas_win_osc_sweep(&s, o, PG_OSC_SINE, t0, 380.0f, 640.0f, 0.45f);
    CasParam og = cas_param(&s, 0.0f, 5);
    cas_ad(&og, t0, 0.12f, 0.14f, 0.4f);
    cas_win_gain(&s, o, &og);
    cas_win_mix(&s, o, 1.0f);
    break;
  }
  case CAS_UI_LOWHEALTH: {
    CasWin w = cas_win(&s, t0, 0.05f + 0.55f + 0.002f);
    cas_win_osc_c(&s, w, PG_OSC_SINE, 92.0f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 0.45f, 0.05f, 0.55f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
    break;
  }
  default: {
    CasWin w = cas_win(&s, t0, 0.03f + 0.002f);
    cas_win_osc_c(&s, w, PG_OSC_SINE, 1200.0f);
    CasParam g = cas_param(&s, 0.0f, 3);
    cas_hit(&g, t0, 0.26f, 0.03f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
    break;
  }
  }
  return s;
}
