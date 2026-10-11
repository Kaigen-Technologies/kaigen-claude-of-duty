// Claude of Duty audio synthesis kit: the synthesis nodes the voices are built from, rendered offline per clip.

#define CAS_SR PG_AUDIO_RATE
#define CAS_FLOOR 0.0001f // the exponential envelopes' resting level
#define CAS_PERIODIC_NORM 0.8482f // saw and square are scaled by the peak of a 2047-partial band-limited table (Gibbs overshoot)
#define CAS_CRACKLE_PEAK 0.514f   // crackle normalises its 2.4 s buffer to 0.9; the grains' peak there is ~1.75

typedef enum CasNoise {
  CAS_NOISE_WHITE = 0,
  CAS_NOISE_PINK,
  CAS_NOISE_BROWN,
  CAS_NOISE_CRACKLE,
} CasNoise;

typedef enum CasEvKind {
  CAS_EV_SET = 0, // jump to the value
  CAS_EV_LIN,     // linear ramp to the value
  CAS_EV_EXP,     // exponential ramp to the value
  CAS_EV_TARGET,  // exponential approach to the target
} CasEvKind;

typedef struct CasEv {
  u32 kind;
  f32 time;
  f32 value;
  f32 tau;
} CasEv;

// an AudioParam: the base value plus its automation events in time order
typedef struct CasParam {
  f32 base;
  u32 count;
  u32 cap;
  CasEv *ev;
} CasParam;

// one render: the job's scratch allocator, its rng and the mono destination
typedef struct CasSyn {
  Allocator *al;
  CodRng rng;
  u32 sr;
  u32 n;
  f32 *out;
} CasSyn;

// a span of the destination a layer renders into: frames [a, a + len)
typedef struct CasWin {
  u32 a;
  u32 len;
  f32 *x;
} CasWin;

typedef struct CasBiquad {
  f32 b0, b1, b2, a1, a2;
  f32 z1, z2;
} CasBiquad;

typedef struct CasPartial {
  f32 f, q, g, decay;
} CasPartial;

hz_internal f32 cas_range(CasSyn *s, f32 lo, f32 hi) { return cod_rng_range(&s->rng, lo, hi); }
hz_internal f32 cas_unit(CasSyn *s) { return cod_rng_float(&s->rng); }
hz_internal f32 cas_semis(f32 n) { return m_exp2f(n / 12.0f); }
hz_internal f32 cas_lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
hz_internal f32 cas_clamp(f32 v, f32 lo, f32 hi) { return v < lo ? lo : (v > hi ? hi : v); }

// the first frame at or after t
hz_internal u32 cas_frame_at(u32 sr, u32 n, f32 t) {
  if (t <= 0.0f)
    return 0;
  const f32 f = m_ceilf(t * (f32)sr - 1e-3f);
  return f >= (f32)n ? n : (u32)f;
}

hz_internal u32 cas_frame(const CasSyn *s, f32 t) { return cas_frame_at(s->sr, s->n, t); }

/* ------------------------------------------------------------ AudioParam automation */

hz_internal CasParam cas_param(CasSyn *s, f32 base, u32 cap) {
  return (CasParam){.base = base, .cap = cap, .ev = ALLOC_ARRAY(s->al, CasEv, cap)};
}

// events keep time order; equal times keep their call order
hz_internal void cas_ev(CasParam *p, u32 kind, f32 value, f32 time, f32 tau) {
  assert_msg(p->count < p->cap, "cod_audio: a param overflows its event list");
  u32 at = p->count;
  while (at > 0 && p->ev[at - 1].time > time) {
    p->ev[at] = p->ev[at - 1];
    at--;
  }
  p->ev[at] = (CasEv){.kind = kind, .time = time, .value = value, .tau = tau};
  p->count++;
}

hz_internal void cas_set(CasParam *p, f32 v, f32 t) { cas_ev(p, CAS_EV_SET, v, t, 0.0f); }
hz_internal void cas_exp(CasParam *p, f32 v, f32 t) { cas_ev(p, CAS_EV_EXP, v, t, 0.0f); }
hz_internal void cas_target(CasParam *p, f32 v, f32 t, f32 tau) { cas_ev(p, CAS_EV_TARGET, v, t, tau); }

hz_internal b32 cas_ok(f32 t0, f32 peak) { return t0 >= 0.0f && t0 < 1e6f && peak == peak && m_absf(peak) < 1e9f; }

// instant attack, exponential decay, silent 2 ms after
hz_internal void cas_hit(CasParam *p, f32 t0, f32 peak, f32 decay) {
  if (!cas_ok(t0, peak))
    return;
  cas_set(p, m_maxf(peak, CAS_FLOOR * 4.0f), t0);
  cas_exp(p, CAS_FLOOR, t0 + decay);
  cas_set(p, 0.0f, t0 + decay + 0.002f);
}

// exponential attack and decay
hz_internal void cas_ad(CasParam *p, f32 t0, f32 peak, f32 attack, f32 decay) {
  if (!cas_ok(t0, peak))
    return;
  const f32 pk = m_maxf(peak, CAS_FLOOR * 4.0f);
  cas_set(p, CAS_FLOOR, t0);
  if (attack > 0.0008f)
    cas_exp(p, pk, t0 + attack);
  else
    cas_set(p, pk, t0 + 0.0004f);
  cas_exp(p, CAS_FLOOR, t0 + attack + decay);
  cas_set(p, 0.0f, t0 + attack + decay + 0.002f);
}

// adsr
hz_internal void cas_adsr(CasParam *p, f32 t0, f32 peak, f32 a, f32 d, f32 sus, f32 sus_level, f32 r) {
  if (!cas_ok(t0, peak))
    return;
  const f32 pk = m_maxf(peak, CAS_FLOOR * 4.0f);
  const f32 sl = m_maxf(pk * sus_level, CAS_FLOOR * 4.0f);
  cas_set(p, CAS_FLOOR, t0);
  cas_exp(p, pk, t0 + a);
  cas_exp(p, sl, t0 + a + d);
  cas_set(p, sl, t0 + a + d + sus);
  cas_exp(p, CAS_FLOOR, t0 + a + d + sus + r);
  cas_set(p, 0.0f, t0 + a + d + sus + r + 0.002f);
}

// exponential from `from` to `to`
hz_internal void cas_sweep(CasParam *p, f32 t0, f32 from, f32 to, f32 dur) {
  if (!cas_ok(t0, from))
    return;
  cas_set(p, m_maxf(from, 1e-3f), t0);
  cas_exp(p, m_maxf(to, 1e-3f), t0 + m_maxf(dur, 0.001f));
}

// the value a segment holds at time t: state is the last event (t0, v0, an optional target curve), `next` the event after it
typedef struct CasSeg {
  f64 t0, v0;
  b32 target;
  f64 tv, tau;
} CasSeg;

hz_internal f64 cas_seg_value(const CasSeg *g, const CasEv *next, f64 t) {
  if (g->target && !(next && (next->kind == CAS_EV_LIN || next->kind == CAS_EV_EXP)))
    return g->tv + (g->v0 - g->tv) * m_exp(-(t - g->t0) / g->tau);
  if (next && next->time > g->t0) {
    const f64 u = (t - g->t0) / ((f64)next->time - g->t0);
    if (next->kind == CAS_EV_LIN)
      return g->v0 + ((f64)next->value - g->v0) * u;
    if (next->kind == CAS_EV_EXP && g->v0 * (f64)next->value > 0.0)
      return g->v0 * m_exp(m_log((f64)next->value / g->v0) * u);
  }
  return g->v0;
}

// the param's value at frames [fa, fa + count) of an n-frame render, into out
hz_internal void cas_param_fill(const CasParam *p, u32 sr, u32 n, u32 fa, u32 count, f32 *out) {
  const u32 fb = fa + count;
  const f64 srd = (f64)sr;
  CasSeg g = {.t0 = 0.0, .v0 = p->base};
  u32 i = fa;
  for (u32 k = 0; k <= p->count && i < fb; k++) {
    const CasEv *e = k < p->count ? &p->ev[k] : NULL;
    u32 end = e ? cas_frame_at(sr, n, e->time) : n;
    if (end > fb)
      end = fb;
    for (; i < end; i++)
      out[i - fa] = (f32)cas_seg_value(&g, e, (f64)i / srd);
    if (!e)
      break;
    if (e->kind == CAS_EV_TARGET) {
      const f64 v = cas_seg_value(&g, e, (f64)e->time);
      g = (CasSeg){.t0 = e->time, .v0 = v, .target = true, .tv = e->value, .tau = m_maxf((f64)e->tau, 1e-4)};
    } else {
      g = (CasSeg){.t0 = e->time, .v0 = e->value};
    }
  }
  for (; i < fb; i++)
    out[i - fa] = (f32)cas_seg_value(&g, NULL, (f64)i / srd);
}

/* ------------------------------------------------------------ windows */

hz_internal CasWin cas_win(CasSyn *s, f32 t0, f32 dur) {
  CasWin w = {.a = cas_frame(s, t0)};
  const u32 want = (u32)m_ceilf(m_maxf(dur, 0.0f) * (f32)s->sr);
  w.len = w.a + want > s->n ? s->n - w.a : want;
  if (w.len)
    w.x = ALLOC_ARRAY(s->al, f32, w.len);
  return w;
}

// adds the window into the destination
hz_internal void cas_win_mix(CasSyn *s, CasWin w, f32 gain) {
  for (u32 i = 0; i < w.len; i++)
    s->out[w.a + i] += w.x[i] * gain;
}

// multiplies by a gain param: the node's value outside its events is the base
hz_internal void cas_win_gain(CasSyn *s, CasWin w, const CasParam *g) {
  if (!w.len)
    return;
  f32 *v = ALLOC_ARRAY(s->al, f32, w.len);
  cas_param_fill(g, s->sr, s->n, w.a, w.len, v);
  for (u32 i = 0; i < w.len; i++)
    w.x[i] *= v[i];
}

hz_internal void cas_win_scale(CasWin w, f32 gain) {
  for (u32 i = 0; i < w.len; i++)
    w.x[i] *= gain;
}

/* ------------------------------------------------------------ noise sources */

typedef struct CasGrain {
  f32 amp, w, dec;
  u32 start;
} CasGrain;

typedef struct CasNoiseGen {
  u32 kind;
  f32 b0, b1, b2, b3, b4, b5, b6;
  f32 last;
  u32 k; // source sample index
  u32 next_grain;
  FixedArray(CasGrain, 20) grains;
} CasNoiseGen;

hz_internal f32 cas_noise_signed(CasSyn *s) { return (f32)(cod_rng_f64(&s->rng) * 2.0 - 1.0); }

// the next sample of a colour
hz_internal f32 cas_noise_next(CasSyn *s, CasNoiseGen *g) {
  switch (g->kind) {
  case CAS_NOISE_PINK: {
    const f32 w = cas_noise_signed(s);
    g->b0 = 0.99886f * g->b0 + w * 0.0555179f;
    g->b1 = 0.99332f * g->b1 + w * 0.0750759f;
    g->b2 = 0.969f * g->b2 + w * 0.153852f;
    g->b3 = 0.8665f * g->b3 + w * 0.3104856f;
    g->b4 = 0.55f * g->b4 + w * 0.5329522f;
    g->b5 = -0.7616f * g->b5 - w * 0.016898f;
    const f32 out = (g->b0 + g->b1 + g->b2 + g->b3 + g->b4 + g->b5 + g->b6 + w * 0.5362f) * 0.11f;
    g->b6 = w * 0.115926f;
    return out;
  }
  case CAS_NOISE_BROWN: {
    const f32 w = cas_noise_signed(s);
    g->last = (g->last + 0.019f * w) * 0.9985f;
    return g->last * 5.2f;
  }
  case CAS_NOISE_CRACKLE: {
    const u32 k = g->k;
    if (k >= g->next_grain && g->grains.len < 20) {
      const f32 amp = cas_range(s, 0.25f, 1.0f) * (cas_unit(s) < 0.12f ? 1.8f : 0.7f);
      const f32 w = cas_range(s, 0.05f, 0.45f);
      const f32 dec = m_expf(-cas_range(s, 0.004f, 0.05f));
      g->grains.items[g->grains.len++] = (CasGrain){amp, w, dec, k};
      g->next_grain = k + 12u + (cod_rng_u32(&s->rng) % 260u);
    }
    f32 out = 0.0f;
    for (u32 i = 0; i < g->grains.len;) {
      CasGrain *gr = &g->grains.items[i];
      const u32 kk = k - gr->start;
      out += m_sinf(gr->w * (f32)kk) * gr->amp;
      gr->amp *= gr->dec;
      if (gr->amp < 1e-4f || kk >= 219u)
        *gr = g->grains.items[--g->grains.len];
      else
        i++;
    }
    return out * CAS_CRACKLE_PEAK;
  }
  default: return cas_noise_signed(s);
  }
}

// a noise source read at `rate`, played from the window's start for its length
hz_internal void cas_win_noise(CasSyn *s, CasWin w, u32 kind, f32 rate) {
  if (!w.len)
    return;
  CasNoiseGen *g = ALLOC(s->al, CasNoiseGen);
  g->kind = kind;
  g->next_grain = 12u + (cod_rng_u32(&s->rng) % 260u);
  if (kind == CAS_NOISE_PINK || kind == CAS_NOISE_BROWN)
    for (u32 i = 0; i < 4000; i++)
      (void)cas_noise_next(s, g);
  f64 pos = 0.0;
  u32 have = 0; // source samples generated: s0 is index have-2, s1 index have-1
  f32 s0 = cas_noise_next(s, g), s1 = cas_noise_next(s, g);
  g->k = 2;
  have = 2;
  for (u32 i = 0; i < w.len; i++) {
    const u32 idx = (u32)pos;
    while (have < idx + 2) {
      s0 = s1;
      s1 = cas_noise_next(s, g);
      g->k = ++have;
    }
    const f32 mu = (f32)(pos - (f64)idx);
    w.x[i] = s0 + (s1 - s0) * mu;
    pos += (f64)rate;
  }
}

// the window length a source of `dur` buffer seconds at `rate` plays for, clipped to the envelope's end
hz_internal f32 cas_src_len(f32 dur, f32 rate, f32 env_end) { return m_minf(dur / m_maxf(rate, 0.01f), env_end); }

/* ------------------------------------------------------------ oscillators */

// an oscillator over the window from a frequency param (or a constant); waves start at 0 rising, square at +1
hz_internal void cas_win_osc(CasSyn *s, CasWin w, PgOscShape shape, const CasParam *freq, f32 detune_cents) {
  if (!w.len)
    return;
  f32 *f = NULL;
  if (freq->count) {
    f = ALLOC_ARRAY(s->al, f32, w.len);
    cas_param_fill(freq, s->sr, s->n, w.a, w.len, f);
  }
  const f32 detune = m_exp2f(detune_cents / 1200.0f);
  const f32 norm = (shape == PG_OSC_SAW || shape == PG_OSC_SQUARE) ? CAS_PERIODIC_NORM : 1.0f;
  const f32 phase0 = shape == PG_OSC_SAW ? 0.5f : (shape == PG_OSC_TRI ? 0.75f : 0.0f);
  const f32 f0 = (f ? f[0] : freq->base) * detune;
  PgOsc o = {.phase = phase0 - f0 / (f32)s->sr}; // pg_osc_blep_next advances before it reads
  if (o.phase < 0.0f)
    o.phase += 1.0f;
  for (u32 i = 0; i < w.len; i++)
    w.x[i] = pg_osc_blep_next(&o, s->sr, (f ? f[i] : freq->base) * detune, shape) * norm;
}

hz_internal void cas_win_osc_c(CasSyn *s, CasWin w, PgOscShape shape, f32 hz) {
  const CasParam p = {.base = hz};
  cas_win_osc(s, w, shape, &p, 0.0f);
}

// an exponential-sweep oscillator: sweep(freq, t0, from, to, dur) over the window starting at t0
hz_internal void cas_win_osc_sweep(CasSyn *s, CasWin w, PgOscShape shape, f32 t0, f32 from, f32 to, f32 dur) {
  CasParam f = cas_param(s, from, 2);
  cas_sweep(&f, t0, from, to, dur);
  cas_win_osc(s, w, shape, &f, 0.0f);
}

/* ------------------------------------------------------------ filters */

// lowpass/highpass Q is in dB, bandpass Q is linear
hz_internal f32 cas_q(PgSvfMode mode, f32 q) { return mode == PG_SVF_BANDPASS ? q : pg_q_from_db(q); }

hz_internal f32 cas_biquad_freq(f32 hz) { return cas_clamp(hz, 10.0f, m_minf(20000.0f, (f32)CAS_SR * 0.48f)); }

hz_internal void cas_win_svf_c(CasWin w, PgSvfMode mode, f32 hz, f32 q) {
  if (!w.len)
    return;
  PgSvf f = {0};
  pg_svf_set(&f, CAS_SR, mode, cas_biquad_freq(hz), cas_q(mode, q));
  pg_svf_process(&f, w.x, w.len);
}

// a filter whose cutoff follows a param
hz_internal void cas_win_svf(CasSyn *s, CasWin w, PgSvfMode mode, const CasParam *freq, f32 q) {
  if (!w.len)
    return;
  if (!freq->count) {
    cas_win_svf_c(w, mode, freq->base, q);
    return;
  }
  f32 *f = ALLOC_ARRAY(s->al, f32, w.len);
  cas_param_fill(freq, s->sr, s->n, w.a, w.len, f);
  PgSvf svf = {0};
  const f32 ql = cas_q(mode, q);
  for (u32 i = 0; i < w.len; i++) {
    pg_svf_set(&svf, s->sr, mode, f[i], ql);
    w.x[i] = pg_svf_next(&svf, w.x[i]);
  }
}

// a filter whose cutoff sweeps exponentially from `from` to `to` starting at t0
hz_internal void cas_win_svf_sweep(CasSyn *s, CasWin w, PgSvfMode mode, f32 t0, f32 from, f32 to, f32 dur, f32 q) {
  CasParam f = cas_param(s, from, 2);
  cas_sweep(&f, t0, from, to, dur);
  cas_win_svf(s, w, mode, &f, q);
}

// both cutoff and Q follow params (the formant bank)
hz_internal void cas_win_svf_fq(CasSyn *s, CasWin w, PgSvfMode mode, const CasParam *freq, const CasParam *q) {
  if (!w.len)
    return;
  f32 *f = ALLOC_ARRAY(s->al, f32, w.len);
  f32 *qq = ALLOC_ARRAY(s->al, f32, w.len);
  cas_param_fill(freq, s->sr, s->n, w.a, w.len, f);
  cas_param_fill(q, s->sr, s->n, w.a, w.len, qq);
  PgSvf svf = {0};
  for (u32 i = 0; i < w.len; i++) {
    pg_svf_set(&svf, s->sr, mode, f[i], cas_q(mode, qq[i]));
    w.x[i] = pg_svf_next(&svf, w.x[i]);
  }
}

// RBJ cookbook biquads
hz_internal void cas_biquad_set(CasBiquad *q, f32 b0, f32 b1, f32 b2, f32 a0, f32 a1, f32 a2) {
  const f32 inv = 1.0f / a0;
  q->b0 = b0 * inv;
  q->b1 = b1 * inv;
  q->b2 = b2 * inv;
  q->a1 = a1 * inv;
  q->a2 = a2 * inv;
}

hz_internal void cas_biquad_peaking(CasBiquad *q, f32 hz, f32 qual, f32 gain_db) {
  const f32 a = m_powf(10.0f, gain_db / 40.0f);
  const f32 w0 = 2.0f * M_PI * cas_biquad_freq(hz) / (f32)CAS_SR;
  const f32 c = m_cosf(w0), alpha = m_sinf(w0) / (2.0f * m_maxf(qual, 1e-3f));
  cas_biquad_set(q, 1.0f + alpha * a, -2.0f * c, 1.0f - alpha * a, 1.0f + alpha / a, -2.0f * c, 1.0f - alpha / a);
}

// slope S = 1 shelf
hz_internal void cas_biquad_highshelf(CasBiquad *q, u32 sr, f32 hz, f32 gain_db) {
  const f32 a = m_powf(10.0f, gain_db / 40.0f);
  const f32 w0 = 2.0f * M_PI * m_clampf(hz, 10.0f, (f32)sr * 0.48f) / (f32)sr;
  const f32 c = m_cosf(w0), sn = m_sinf(w0);
  const f32 beta = 2.0f * m_sqrtf(a) * (sn * 0.70710678f);
  cas_biquad_set(q, a * ((a + 1.0f) + (a - 1.0f) * c + beta), -2.0f * a * ((a - 1.0f) + (a + 1.0f) * c),
                 a * ((a + 1.0f) + (a - 1.0f) * c - beta), (a + 1.0f) - (a - 1.0f) * c + beta,
                 2.0f * ((a - 1.0f) - (a + 1.0f) * c), (a + 1.0f) - (a - 1.0f) * c - beta);
}

hz_internal f32 cas_biquad_next(CasBiquad *q, f32 x) {
  const f32 y = q->b0 * x + q->z1;
  q->z1 = q->b1 * x - q->a1 * y + q->z2;
  q->z2 = q->b2 * x - q->a2 * y;
  return y;
}

hz_internal void cas_win_biquad(CasWin w, CasBiquad *q) {
  for (u32 i = 0; i < w.len; i++)
    w.x[i] = cas_biquad_next(q, w.x[i]);
}

hz_internal void cas_win_peaking(CasWin w, f32 hz, f32 qual, f32 gain_db) {
  CasBiquad q = {0};
  cas_biquad_peaking(&q, hz, qual, gain_db);
  cas_win_biquad(w, &q);
}

/* ------------------------------------------------------------ waveshaper */

// tanh(k x') / tanh(k), k = 1 + drive, x' = x + asym x^2 sign(x) / 2
hz_internal f32 *cas_curve(CasSyn *s, f32 drive, f32 asym) {
  f32 *c = ALLOC_ARRAY(s->al, f32, 2048);
  const f32 k = 1.0f + drive;
  const f32 norm = 1.0f / m_tanhf(k);
  for (u32 i = 0; i < 2048; i++) {
    const f32 x = (f32)i / 2047.0f * 2.0f - 1.0f;
    const f32 xa = x + asym * x * x * (x < 0.0f ? -1.0f : 1.0f) * 0.5f;
    c[i] = m_tanhf(k * xa) * norm;
  }
  return c;
}

// the limiter curve is applied by the mixer; this is the voice-side lookup with linear interpolation
hz_internal f32 cas_curve_lookup(const f32 *c, u32 count, f32 x) {
  if (x <= -1.0f)
    return c[0];
  if (x >= 1.0f)
    return c[count - 1];
  const f32 v = (f32)(count - 1) * 0.5f * (x + 1.0f);
  const u32 k = (u32)v;
  const f32 t = v - (f32)k;
  return c[k] + (c[k + 1] - c[k]) * t;
}

#define CAS_HB_TAPS 8 // half-band polyphase taps per side: 16 taps

hz_internal f32 cas_hb_coef(u32 j) {
  const f32 k = (f32)(2u * j + 1u);
  const f32 span = (f32)(2u * CAS_HB_TAPS);
  const f32 wn = 0.42f + 0.5f * m_cosf(M_PI * k / span) + 0.08f * m_cosf(2.0f * M_PI * k / span); // Blackman
  return ((j & 1u) ? -1.0f : 1.0f) / (M_PI * k) * wn;
}

// 2x up: even outputs copy the input, odd ones interpolate with a half-band FIR (gain 2 on the zero-stuffed signal)
hz_internal void cas_upsample2(const f32 *in, u32 n, f32 *out) {
  for (u32 i = 0; i < n; i++) {
    out[2 * i] = in[i];
    f32 acc = 0.0f;
    for (u32 j = 0; j < CAS_HB_TAPS; j++) {
      const f32 c = 2.0f * cas_hb_coef(j);
      const f32 lo = (i >= j) ? in[i - j] : 0.0f;
      const f32 hi = (i + 1 + j < n) ? in[i + 1 + j] : 0.0f;
      acc += c * (lo + hi);
    }
    out[2 * i + 1] = acc;
  }
}

hz_internal void cas_downsample2(const f32 *in, u32 n_out, f32 *out) {
  const u32 n = 2 * n_out;
  for (u32 i = 0; i < n_out; i++) {
    f32 acc = 0.5f * in[2 * i];
    for (u32 j = 0; j < CAS_HB_TAPS; j++) {
      const u32 d = 2 * j + 1;
      const f32 lo = (2 * i >= d) ? in[2 * i - d] : 0.0f;
      const f32 hi = (2 * i + d < n) ? in[2 * i + d] : 0.0f;
      acc += cas_hb_coef(j) * (lo + hi);
    }
    out[i] = acc;
  }
}

// the shaper node with oversample 1, 2 or 4
hz_internal void cas_win_shape(CasSyn *s, CasWin w, const f32 *curve, u32 over) {
  if (!w.len)
    return;
  if (over <= 1) {
    for (u32 i = 0; i < w.len; i++)
      w.x[i] = cas_curve_lookup(curve, 2048, w.x[i]);
    return;
  }
  TempAllocator ta = tctx_temp_allocator_begin(NULL);
  const u32 stages = over >= 4 ? 2 : 1;
  const u32 top = w.len << stages;
  f32 *up1 = ALLOC_ARRAY(&ta.allocator, f32, w.len * 2u);
  f32 *up2 = stages == 2 ? ALLOC_ARRAY(&ta.allocator, f32, top) : up1;
  cas_upsample2(w.x, w.len, up1);
  if (stages == 2)
    cas_upsample2(up1, w.len * 2u, up2);
  for (u32 i = 0; i < top; i++)
    up2[i] = cas_curve_lookup(curve, 2048, up2[i]);
  if (stages == 2)
    cas_downsample2(up2, w.len * 2u, up1);
  cas_downsample2(up1, w.len, w.x);
  tctx_temp_allocator_end(ta);
}

/* ------------------------------------------------------------ struck resonator */

// a short noise burst through a bank of high-Q bandpasses, each with its own decay
hz_internal void cas_struck(CasSyn *s, f32 t0, const CasPartial *parts, u32 count, f32 excite_dur) {
  if (t0 < 0.0f || t0 >= (f32)s->n / (f32)s->sr)
    return;
  const f32 rate = cas_range(s, 0.85f, 1.2f);
  f32 longest = 0.0f;
  for (u32 p = 0; p < count; p++)
    longest = m_maxf(longest, parts[p].decay);
  CasWin ex = cas_win(s, t0, longest + 0.004f);
  if (!ex.len)
    return;
  CasWin burst = cas_win(s, t0, excite_dur + 0.002f + 0.0f);
  cas_win_noise(s, burst, CAS_NOISE_WHITE, rate);
  CasParam eg = cas_param(s, 0.0f, 3);
  cas_hit(&eg, t0, 1.0f, excite_dur);
  cas_win_gain(s, burst, &eg);
  for (u32 p = 0; p < count; p++) {
    const CasPartial *pt = &parts[p];
    CasWin w = cas_win(s, t0, pt->decay + 0.002f);
    if (!w.len)
      continue;
    for (u32 i = 0; i < w.len && i < burst.len; i++)
      w.x[i] = burst.x[i];
    cas_win_svf_c(w, PG_SVF_BANDPASS, pt->f, pt->q);
    CasParam vg = cas_param(s, 0.0f, 3);
    cas_hit(&vg, t0, pt->g * m_sqrtf(pt->q) * 0.85f, pt->decay);
    cas_win_gain(s, w, &vg);
    cas_win_mix(s, w, 1.0f);
  }
}

// a struck resonator from up to four partials given by value
hz_internal void cas_struck4(CasSyn *s, f32 t0, f32 excite_dur, u32 count, CasPartial a, CasPartial b, CasPartial c, CasPartial d) {
  CasPartial *p = ALLOC_ARRAY(s->al, CasPartial, 4);
  p[0] = a;
  p[1] = b;
  p[2] = c;
  p[3] = d;
  cas_struck(s, t0, p, count, excite_dur);
}

hz_internal void cas_struck1(CasSyn *s, f32 t0, f32 excite_dur, CasPartial a) {
  cas_struck4(s, t0, excite_dur, 1, a, a, a, a);
}

/* ------------------------------------------------------------ job plumbing */

hz_internal CasSyn cas_syn_begin(Allocator *al, u64 seed, u32 n) {
  CasSyn s = {.al = al, .rng = cod_rng_make((u32)(seed ^ (seed >> 32))), .sr = CAS_SR, .n = n};
  s.out = ALLOC_ARRAY(al, f32, n);
  return s;
}

// the render's highest-frequency air loss at distance d
hz_internal f32 cas_air_cutoff(f32 d) { return cas_clamp(20500.0f / (1.0f + d * 0.055f), 260.0f, 20000.0f); }

// a lowpass and a high shelf over the whole render: the baked air absorption and occlusion
hz_internal void cas_apply_filter(f32 *x, u32 n, f32 lp_hz, f32 shelf_db) {
  if (lp_hz < 19000.0f) {
    PgSvf f = {0};
    pg_svf_set(&f, CAS_SR, PG_SVF_LOWPASS, lp_hz, pg_q_from_db(0.5f));
    pg_svf_process(&f, x, n);
  }
  if (shelf_db < -0.01f) {
    CasBiquad q = {0};
    cas_biquad_highshelf(&q, CAS_SR, 2200.0f, shelf_db);
    for (u32 i = 0; i < n; i++)
      x[i] = cas_biquad_next(&q, x[i]);
  }
}
