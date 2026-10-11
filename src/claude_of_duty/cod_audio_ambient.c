// Claude of Duty audio, ambience one-shots (dog, siren, creak, settle, birds, vehicle, heli, shout).

typedef enum CasAmbient {
  CAS_AMB_DOG = 0,
  CAS_AMB_SIREN,
  CAS_AMB_CREAK,
  CAS_AMB_SETTLE,
  CAS_AMB_BIRDS,
  CAS_AMB_VEHICLE,
  CAS_AMB_HELI,
  CAS_AMB_SHOUT,
  CAS_AMB_COUNT,
} CasAmbient;

// the kinds heard from afar (the scheduler places them 90 to 260 m out); the rest are placed 14 to 90 m out
hz_internal b32 cas_ambient_far(u32 kind) { return kind == CAS_AMB_SIREN || kind == CAS_AMB_HELI; }

hz_internal f32 cas_ambient_seconds(u32 kind) {
  switch (kind) {
  case CAS_AMB_DOG: return 1.4f;
  case CAS_AMB_SIREN: return 9.6f;
  case CAS_AMB_CREAK: return 3.6f;
  case CAS_AMB_SETTLE: return 1.2f;
  case CAS_AMB_BIRDS: return 1.9f;
  case CAS_AMB_VEHICLE: return 7.1f;
  case CAS_AMB_HELI: return 12.1f;
  default: return 1.0f;
  }
}

// at level 1, band-limited to the air absorption of `distance` metres (the scheduler's range for the kind)
hz_internal CasSyn cas_voice_ambient(Allocator *al, u64 seed, u32 kind, f32 distance) {
  CasSyn s = cas_syn_begin(al, seed, (u32)m_ceilf(cas_ambient_seconds(kind) * (f32)CAS_SR));
  const f32 t0 = 0.0f;
  switch (kind) {
  case CAS_AMB_DOG: {
    const u32 n = 2u + (cod_rng_u32(&s.rng) % 2u);
    for (u32 i = 0; i < n; i++) {
      const f32 bt = t0 + (f32)i * cas_range(&s, 0.24f, 0.44f);
      const f32 f_from = cas_range(&s, 300.0f, 420.0f), f_to = cas_range(&s, 150.0f, 220.0f);
      (void)cas_range(&s, 220.0f, 340.0f); // the oscillator's constructor frequency, replaced by the sweep
      CasWin w = cas_win(&s, bt, 0.01f + 0.1f + 0.002f);
      cas_win_osc_sweep(&s, w, PG_OSC_SAW, bt, f_from, f_to, 0.11f);
      cas_win_svf_c(w, PG_SVF_BANDPASS, cas_range(&s, 700.0f, 1200.0f), 2.2f);
      CasParam g = cas_param(&s, 0.0f, 5);
      cas_ad(&g, bt, 0.5f, 0.01f, 0.1f);
      cas_win_gain(&s, w, &g);
      cas_win_mix(&s, w, 1.0f);
      CasWin nz = cas_win(&s, bt, cas_src_len(0.2f, 1.0f, 0.008f + 0.08f + 0.002f));
      cas_win_noise(&s, nz, CAS_NOISE_WHITE, 1.0f);
      cas_win_svf_c(nz, PG_SVF_BANDPASS, 2400.0f, 1.2f);
      CasParam ng = cas_param(&s, 0.0f, 5);
      cas_ad(&ng, bt, 0.12f, 0.008f, 0.08f);
      cas_win_gain(&s, nz, &ng);
      cas_win_mix(&s, nz, 1.0f);
    }
    break;
  }
  case CAS_AMB_SIREN: {
    const f32 dur = cas_range(&s, 4.0f, 9.0f);
    const f32 wob_hz = cas_range(&s, 0.35f, 0.6f);
    CasWin w = cas_win(&s, t0, dur + 0.002f);
    f32 p1 = 0.0f, p2 = 0.0f; // two sines at 620 and 930 Hz, frequency-modulated by the same wobble
    for (u32 i = 0; i < w.len; i++) {
      const f32 t = (f32)(w.a + i) / (f32)s.sr;
      const f32 wob = 110.0f * m_sinf(2.0f * M_PI * wob_hz * t);
      p1 += (620.0f + wob) / (f32)s.sr;
      p2 += (930.0f + wob) / (f32)s.sr;
      p1 -= m_floorf(p1);
      p2 -= m_floorf(p2);
      w.x[i] = m_sinf(2.0f * M_PI * p1) + m_sinf(2.0f * M_PI * p2);
    }
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 0.022f, dur * 0.3f, dur * 0.7f);
    cas_win_gain(&s, w, &g);
    cas_win_svf_c(w, PG_SVF_LOWPASS, 1800.0f, 0.8f);
    cas_win_mix(&s, w, 1.0f);
    break;
  }
  case CAS_AMB_CREAK: {
    const f32 dur = cas_range(&s, 0.9f, 2.4f);
    const f32 rate = cas_range(&s, 0.6f, 1.0f);
    CasWin w = cas_win(&s, t0, cas_src_len(dur * 1.5f, rate, dur * 0.3f + dur * 0.8f + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
    cas_win_svf_sweep(&s, w, PG_SVF_BANDPASS, t0, cas_range(&s, 500.0f, 900.0f), cas_range(&s, 1100.0f, 2200.0f), dur, 22.0f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 0.3f, dur * 0.3f, dur * 0.8f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
    cas_struck1(&s, t0 + dur * 0.9f, 0.003f, (CasPartial){cas_range(&s, 400.0f, 1400.0f), 20.0f, 0.18f, 0.1f});
    break;
  }
  case CAS_AMB_SETTLE: {
    for (u32 i = 0; i < 7; i++)
      cas_struck1(&s, t0 + cas_range(&s, 0.0f, 0.7f), 0.002f,
                  (CasPartial){cas_range(&s, 600.0f, 5000.0f), cas_range(&s, 8.0f, 26.0f), cas_range(&s, 0.02f, 0.09f),
                               cas_range(&s, 0.01f, 0.07f)});
    CasWin w = cas_win(&s, t0, 0.01f + 0.16f + 0.002f);
    cas_win_osc_sweep(&s, w, PG_OSC_SINE, t0, 110.0f, 55.0f, 0.15f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 0.14f, 0.01f, 0.16f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
    break;
  }
  case CAS_AMB_BIRDS: {
    const u32 n = 3u + (cod_rng_u32(&s.rng) % 5u);
    for (u32 i = 0; i < n; i++) {
      const f32 bt = t0 + cas_range(&s, 0.0f, 1.4f);
      const b32 up = cas_unit(&s) < 0.5f;
      CasWin w = cas_win(&s, bt, 0.008f + 0.06f + 0.002f);
      cas_win_osc_sweep(&s, w, PG_OSC_SINE, bt, up ? 2600.0f : 4400.0f, up ? 4600.0f : 2700.0f, 0.06f);
      CasParam g = cas_param(&s, 0.0f, 5);
      cas_ad(&g, bt, 0.05f, 0.008f, 0.06f);
      cas_win_gain(&s, w, &g);
      cas_win_mix(&s, w, 1.0f);
    }
    break;
  }
  case CAS_AMB_VEHICLE: {
    const f32 dur = cas_range(&s, 3.5f, 7.0f);
    const f32 rate = cas_range(&s, 0.7f, 1.0f);
    CasWin w = cas_win(&s, t0, cas_src_len(dur * 1.2f, rate, dur * 0.45f + dur * 0.55f + 0.002f));
    cas_win_noise(&s, w, CAS_NOISE_BROWN, rate);
    CasParam lp = cas_param(&s, 300.0f, 6);
    cas_sweep(&lp, t0, 200.0f, 460.0f, dur * 0.5f);
    cas_sweep(&lp, t0 + dur * 0.5f, 460.0f, 180.0f, dur * 0.5f);
    cas_win_svf(&s, w, PG_SVF_LOWPASS, &lp, 0.9f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 0.16f, dur * 0.45f, dur * 0.55f);
    cas_win_gain(&s, w, &g);
    cas_win_mix(&s, w, 1.0f);
    CasWin e = cas_win(&s, t0, dur * 0.45f + dur * 0.55f + 0.002f);
    cas_win_osc_c(&s, e, PG_OSC_SAW, cas_range(&s, 52.0f, 78.0f));
    CasParam eg = cas_param(&s, 0.0f, 5);
    cas_ad(&eg, t0, 0.035f, dur * 0.45f, dur * 0.55f);
    cas_win_gain(&s, e, &eg);
    cas_win_svf_c(e, PG_SVF_LOWPASS, 240.0f, 1.2f);
    cas_win_mix(&s, e, 1.0f);
    break;
  }
  case CAS_AMB_HELI: {
    const f32 dur = cas_range(&s, 6.0f, 12.0f);
    const f32 rate = cas_range(&s, 0.8f, 1.1f);
    const f32 env_end = dur * 0.4f + dur * 0.6f + 0.002f;
    CasWin w = cas_win(&s, t0, cas_src_len(dur * 1.2f, rate, env_end));
    cas_win_noise(&s, w, CAS_NOISE_BROWN, rate);
    cas_win_svf_c(w, PG_SVF_LOWPASS, 420.0f, 0.9f);
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 2.1f, dur * 0.4f, dur * 0.6f);
    cas_win_gain(&s, w, &g);
    const f32 thump_hz = cas_range(&s, 4.6f, 6.4f);
    for (u32 i = 0; i < w.len; i++) {
      const f32 t = (f32)(w.a + i) / (f32)s.sr;
      w.x[i] *= 0.45f + 0.5f * m_sinf(2.0f * M_PI * thump_hz * t); // blade-pass modulation scales the bed
    }
    cas_win_mix(&s, w, 1.0f);
    CasWin wh = cas_win(&s, t0, env_end);
    cas_win_osc_c(&s, wh, PG_OSC_SAW, cas_range(&s, 280.0f, 420.0f));
    cas_win_svf_c(wh, PG_SVF_BANDPASS, 1400.0f, 6.0f);
    CasParam wg = cas_param(&s, 0.0f, 5);
    cas_ad(&wg, t0, 0.11f, dur * 0.4f, dur * 0.6f);
    cas_win_gain(&s, wh, &wg);
    cas_win_mix(&s, wh, 1.0f);
    break;
  }
  default: {
    const f32 dur = cas_range(&s, 0.3f, 0.7f);
    const f32 from = cas_range(&s, 130.0f, 170.0f), to = cas_range(&s, 95.0f, 125.0f);
    (void)cas_range(&s, 110.0f, 160.0f);
    CasWin w = cas_win(&s, t0, 0.05f + dur + 0.002f);
    cas_win_osc_sweep(&s, w, PG_OSC_SAW, t0, from, to, dur);
    CasWin w2 = cas_win(&s, t0, 0.05f + dur + 0.002f);
    for (u32 i = 0; i < w.len; i++)
      w2.x[i] = w.x[i];
    cas_win_svf_c(w, PG_SVF_BANDPASS, cas_range(&s, 600.0f, 900.0f), 4.0f);
    cas_win_svf_c(w2, PG_SVF_BANDPASS, cas_range(&s, 1300.0f, 2000.0f), 5.0f);
    for (u32 i = 0; i < w.len; i++)
      w.x[i] += w2.x[i];
    CasParam g = cas_param(&s, 0.0f, 5);
    cas_ad(&g, t0, 0.2f, 0.05f, dur);
    cas_win_gain(&s, w, &g);
    cas_win_svf_c(w, PG_SVF_LOWPASS, 2600.0f, 0.8f);
    cas_win_mix(&s, w, 1.0f);
    break;
  }
  }
  for (u32 i = 0; i < s.n; i++)
    s.out[i] *= 0.55f; // the voice trim
  cas_apply_filter(s.out, s.n, cas_air_cutoff(distance), 0.0f);
  return s;
}
