// Claude of Duty audio, the ambience bed: wind, whistle, city and distant war, synthesised live so the bed never repeats.

typedef struct CasBedEvent {
  f32 time, target, tau;
} CasBedEvent;

// a parameter eased toward its target: it eases to its target, and scheduled targets take over at their time
typedef struct CasBedParam {
  f32 v, target, k;
  FixedArray(CasBedEvent, 8) ev;
} CasBedParam;

typedef struct CasBedLfo {
  f32 phase, hz, depth, start;
} CasBedLfo;

typedef struct CasBedNoise {
  u32 kind;
  f32 b0, b1, b2, b3, b4, b5, b6, last;
  f64 pos;
  f32 s0, s1;
  f32 rate;
  u32 have;
} CasBedNoise;

typedef struct CasBedWind {
  CasBedNoise noise;
  PgSvf hp, lp;
  CasBedParam gain, lp_hz;
  CasBedLfo g1, g2, f1;
  f32 pan_l, pan_r;
} CasBedWind;

typedef struct CasBed {
  u32 sr;
  CodRng rng;
  f64 t;
  f32 enclosure;
  f32 gust_timer;
  CasBedWind wind[2];
  CasBedNoise whistle_noise, city_noise, war_noise;
  PgSvf whistle_bp, city_hp, city_lp, war_lp;
  CasBedLfo whistle_f, whistle_g, city_g, city_f, war_g;
  CasBedParam wind_gain, outdoor_hz, outdoor_gain;
  PgSvf outdoor_lp[2];
  u32 block; // samples since the last coefficient refresh
} CasBed;

hz_internal f32 cas_bed_k(f32 tau, u32 sr) { return 1.0f - m_expf(-1.0f / (m_maxf(tau, 1e-4f) * (f32)sr)); }

hz_internal void cas_bed_param_target(CasBedParam *p, f32 target, f32 tau, u32 sr) {
  p->target = target;
  p->k = cas_bed_k(tau, sr);
}

// eases toward target with time constant tau, starting `when` seconds from now
hz_internal void cas_bed_param_schedule(CasBedParam *p, f32 when, f32 target, f32 tau, f64 now) {
  if (p->ev.len >= 8)
    return;
  p->ev.items[p->ev.len++] = (CasBedEvent){(f32)now + when, target, tau};
}

hz_internal f32 cas_bed_param_next(CasBedParam *p, f64 now, u32 sr) {
  for (u32 i = 0; i < p->ev.len;) {
    if (p->ev.items[i].time <= (f32)now) {
      cas_bed_param_target(p, p->ev.items[i].target, p->ev.items[i].tau, sr);
      p->ev.items[i] = p->ev.items[--p->ev.len];
    } else {
      i++;
    }
  }
  p->v += (p->target - p->v) * p->k;
  return p->v;
}

hz_internal f32 cas_bed_lfo_next(CasBedLfo *l, f64 now, u32 sr) {
  if ((f32)now < l->start)
    return 0.0f;
  l->phase += l->hz / (f32)sr;
  l->phase -= m_floorf(l->phase);
  return l->depth * m_sinf(2.0f * M_PI * l->phase);
}

hz_internal f32 cas_bed_signed(CodRng *r) { return (f32)(cod_rng_f64(r) * 2.0 - 1.0); }

hz_internal f32 cas_bed_noise_raw(CasBedNoise *n, CodRng *r) {
  if (n->kind == CAS_NOISE_PINK) {
    const f32 w = cas_bed_signed(r);
    n->b0 = 0.99886f * n->b0 + w * 0.0555179f;
    n->b1 = 0.99332f * n->b1 + w * 0.0750759f;
    n->b2 = 0.969f * n->b2 + w * 0.153852f;
    n->b3 = 0.8665f * n->b3 + w * 0.3104856f;
    n->b4 = 0.55f * n->b4 + w * 0.5329522f;
    n->b5 = -0.7616f * n->b5 - w * 0.016898f;
    const f32 out = (n->b0 + n->b1 + n->b2 + n->b3 + n->b4 + n->b5 + n->b6 + w * 0.5362f) * 0.11f;
    n->b6 = w * 0.115926f;
    return out;
  }
  if (n->kind == CAS_NOISE_BROWN) {
    n->last = (n->last + 0.019f * cas_bed_signed(r)) * 0.9985f;
    return n->last * 5.2f;
  }
  return cas_bed_signed(r);
}

// a looping noise buffer's samples read at `rate`: the colour generated on demand, linearly interpolated
hz_internal f32 cas_bed_noise_next(CasBedNoise *n, CodRng *r) {
  if (n->have == 0) {
    for (u32 i = 0; i < 4000; i++)
      (void)cas_bed_noise_raw(n, r);
    n->s0 = cas_bed_noise_raw(n, r);
    n->s1 = cas_bed_noise_raw(n, r);
    n->have = 2;
  }
  const u32 idx = (u32)n->pos;
  while (n->have < idx + 2u) {
    n->s0 = n->s1;
    n->s1 = cas_bed_noise_raw(n, r);
    n->have++;
  }
  const f32 mu = (f32)(n->pos - (f64)idx);
  const f32 out = n->s0 + (n->s1 - n->s0) * mu;
  n->pos += (f64)n->rate;
  if (n->pos > 1e6) { // re-base the position so the double keeps its precision
    n->pos -= (f64)idx;
    n->have -= idx;
  }
  return out;
}

hz_internal CasBedLfo cas_bed_lfo(CodRng *r, f32 hz, f32 depth) {
  return (CasBedLfo){.hz = hz, .depth = depth, .start = cod_rng_range(r, 0.0f, 10.0f)};
}

hz_internal void cas_bed_init(CasBed *b, u32 sr, u32 seed) {
  *b = (CasBed){.sr = sr, .rng = cod_rng_make(seed)};
  CodRng *r = &b->rng;
  for (u32 i = 0; i < 2; i++) {
    CasBedWind *w = &b->wind[i];
    w->noise = (CasBedNoise){.kind = CAS_NOISE_BROWN, .rate = cod_rng_range(r, 0.82f, 1.15f)};
    const f32 lp = cod_rng_range(r, 260.0f, 520.0f);
    w->lp_hz = (CasBedParam){.v = lp, .target = lp, .k = 0.0f};
    w->gain = (CasBedParam){.v = 0.5f, .target = 0.5f, .k = 0.0f};
    const f32 pan = i == 0 ? -0.55f : 0.55f;
    const f32 x = (pan + 1.0f) * 0.5f * (M_PI * 0.5f);
    w->pan_l = m_cosf(x);
    w->pan_r = m_sinf(x);
    w->g1 = cas_bed_lfo(r, 0.041f + (f32)i * 0.017f, cod_rng_range(r, 0.18f, 0.3f));
    w->g2 = cas_bed_lfo(r, 0.0917f + (f32)i * 0.031f, cod_rng_range(r, 0.08f, 0.16f));
    w->f1 = cas_bed_lfo(r, 0.037f + (f32)i * 0.023f, cod_rng_range(r, 80.0f, 170.0f));
    pg_svf_set(&w->hp, sr, PG_SVF_HIGHPASS, 40.0f, pg_q_from_db(0.7f));
  }
  b->whistle_noise = (CasBedNoise){.kind = CAS_NOISE_WHITE, .rate = 1.0f};
  b->city_noise = (CasBedNoise){.kind = CAS_NOISE_PINK, .rate = 0.9f};
  b->war_noise = (CasBedNoise){.kind = CAS_NOISE_BROWN, .rate = 0.7f};
  b->whistle_f = cas_bed_lfo(r, 0.053f, 640.0f);
  b->whistle_g = cas_bed_lfo(r, 0.071f, 0.011f);
  b->city_g = cas_bed_lfo(r, 0.023f, 0.025f);
  b->city_f = cas_bed_lfo(r, 0.0311f, 120.0f);
  b->war_g = cas_bed_lfo(r, 0.0137f, 0.035f);
  pg_svf_set(&b->city_hp, sr, PG_SVF_HIGHPASS, 70.0f, pg_q_from_db(0.7f));
  pg_svf_set(&b->war_lp, sr, PG_SVF_LOWPASS, 105.0f, pg_q_from_db(0.9f));
  b->wind_gain = (CasBedParam){.v = 0.5f, .target = 0.5f};
  b->outdoor_hz = (CasBedParam){.v = 20000.0f, .target = 20000.0f};
  b->outdoor_gain = (CasBedParam){.v = 1.0f, .target = 1.0f};
  b->gust_timer = cod_rng_range(r, 4.0f, 14.0f);
}

// indoors the outdoor content is filtered and dropped
hz_internal void cas_bed_enclosure(CasBed *b, f32 v) {
  b->enclosure = cas_clamp(v, 0.0f, 1.0f);
  cas_bed_param_target(&b->outdoor_hz, cas_lerp(20000.0f, 620.0f, b->enclosure), 0.6f, b->sr);
  cas_bed_param_target(&b->outdoor_gain, cas_lerp(1.0f, 0.45f, b->enclosure), 0.6f, b->sr);
  cas_bed_param_target(&b->wind_gain, cas_lerp(0.5f, 0.12f, b->enclosure), 0.8f, b->sr);
}

// the level swells and the lowpass opens as the air speeds up
hz_internal void cas_bed_gust(CasBed *b) {
  CodRng *r = &b->rng;
  const f32 dur = cod_rng_range(r, 2.2f, 6.5f);
  const f32 strength = cod_rng_range(r, 0.25f, 1.0f) * cas_lerp(1.0f, 0.25f, b->enclosure);
  for (u32 i = 0; i < 2; i++) {
    CasBedWind *w = &b->wind[i];
    const f32 peak = 0.5f + 0.5f * strength * cod_rng_range(r, 0.7f, 1.2f);
    cas_bed_param_schedule(&w->gain, cod_rng_range(r, 0.0f, 0.5f), peak, dur * 0.28f, b->t);
    cas_bed_param_schedule(&w->gain, dur * 0.55f, 0.5f, dur * 0.4f, b->t);
    const f32 f = w->lp_hz.target;
    cas_bed_param_schedule(&w->lp_hz, 0.0f, f * (1.0f + strength * 0.9f), dur * 0.3f, b->t);
    cas_bed_param_schedule(&w->lp_hz, dur * 0.6f, f, dur * 0.5f, b->t);
  }
}

#define CAS_BED_REFRESH 16u

// renders `frames` of the bed (stereo interleaved, before the bus trim) and runs the gust timer
hz_internal void cas_bed_render(CasBed *b, f32 *lr, u32 frames, f32 dt) {
  const u32 sr = b->sr;
  CodRng *r = &b->rng;
  b->gust_timer -= dt;
  if (b->gust_timer <= 0.0f) {
    b->gust_timer = cod_rng_range(r, 5.0f, 16.0f);
    cas_bed_gust(b);
  }
  for (u32 i = 0; i < frames; i++) {
    const f64 now = b->t;
    const b32 refresh = (b->block++ % CAS_BED_REFRESH) == 0;
    f32 wl = 0.0f, wr = 0.0f;
    for (u32 k = 0; k < 2; k++) {
      CasBedWind *w = &b->wind[k];
      f32 x = cas_bed_noise_next(&w->noise, r);
      x = pg_svf_next(&w->hp, x);
      const f32 fhz = cas_bed_param_next(&w->lp_hz, now, sr) + cas_bed_lfo_next(&w->f1, now, sr);
      if (refresh)
        pg_svf_set(&w->lp, sr, PG_SVF_LOWPASS, m_maxf(fhz, 20.0f), pg_q_from_db(0.6f));
      x = pg_svf_next(&w->lp, x);
      const f32 g = cas_bed_param_next(&w->gain, now, sr) + cas_bed_lfo_next(&w->g1, now, sr) + cas_bed_lfo_next(&w->g2, now, sr);
      x *= g;
      wl += x * w->pan_l;
      wr += x * w->pan_r;
    }
    // the whistle through edges and wires joins the wind before the wind gain
    {
      const f32 f = 820.0f + cas_bed_lfo_next(&b->whistle_f, now, sr);
      if (refresh)
        pg_svf_set(&b->whistle_bp, sr, PG_SVF_BANDPASS, m_maxf(f, 20.0f), 7.0f);
      const f32 w = pg_svf_next(&b->whistle_bp, cas_bed_noise_next(&b->whistle_noise, r)) * (0.012f + cas_bed_lfo_next(&b->whistle_g, now, sr));
      wl += w;
      wr += w;
    }
    const f32 wg = cas_bed_param_next(&b->wind_gain, now, sr);
    wl *= wg;
    wr *= wg;
    // the distant city and the war rumble feed the outdoor stage directly
    f32 city = cas_bed_noise_next(&b->city_noise, r);
    city = pg_svf_next(&b->city_hp, city);
    const f32 cf = 480.0f + cas_bed_lfo_next(&b->city_f, now, sr);
    if (refresh)
      pg_svf_set(&b->city_lp, sr, PG_SVF_LOWPASS, m_maxf(cf, 20.0f), pg_q_from_db(0.7f));
    city = pg_svf_next(&b->city_lp, city) * (0.06f + cas_bed_lfo_next(&b->city_g, now, sr));
    const f32 war = pg_svf_next(&b->war_lp, cas_bed_noise_next(&b->war_noise, r)) * (0.05f + cas_bed_lfo_next(&b->war_g, now, sr));
    wl += city + war;
    wr += city + war;
    const f32 ohz = cas_bed_param_next(&b->outdoor_hz, now, sr);
    if (refresh) {
      pg_svf_set(&b->outdoor_lp[0], sr, PG_SVF_LOWPASS, ohz, pg_q_from_db(0.6f));
      pg_svf_set(&b->outdoor_lp[1], sr, PG_SVF_LOWPASS, ohz, pg_q_from_db(0.6f));
    }
    const f32 og = cas_bed_param_next(&b->outdoor_gain, now, sr);
    lr[2u * i] = pg_svf_next(&b->outdoor_lp[0], wl) * og;
    lr[2u * i + 1u] = pg_svf_next(&b->outdoor_lp[1], wr) * og;
    b->t += 1.0 / (f64)sr;
  }
}
