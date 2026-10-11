// Claude of Duty audio, the mixer: buses with trim, ducking and compression, the concussion muffle, tinnitus and the master chain.

// the engine has four fold buses: SFX carries the weapons (and the ambience), MUSIC the foley, VOICE the barks and UI the HUD sounds
#define CAS_BUS_WEAPONS AUDIO_BUS_SFX
#define CAS_BUS_FOLEY AUDIO_BUS_MUSIC
#define CAS_BUS_VOICE AUDIO_BUS_VOICE
#define CAS_BUS_UI AUDIO_BUS_UI
#define CAS_MASTER_VOLUME 0.95f // the settings volume 1.0 maps to it
#define CAS_ENGINE_FINAL 0.8265f // audio_softclip(x) * 0.95 for small x: the engine's master stage, divided out so the mix keeps the level

// the concussion filter shared by every world bus and the reverb return
typedef struct CasMuffle {
  f32 lp_hz;
  f32 hs_db;
  f32 gain;
} CasMuffle;

// a bus: duck gain, static trim, compressor, then the muffle
typedef struct CasBusFx {
  u32 sr;
  f32 trim;
  f32 duck, duck_target, duck_tau;
  b32 has_comp;
  PgCompressor comp;
  const CasMuffle *muffle;
  PgSvf mlp[2];
  CasBiquad mhs[2];
  f32 coef_hz, coef_db;
  f64 meter_sq;
  f32 meter_peak;
  u32 meter_frames;
} CasBusFx;

hz_internal f32 cas_db(f32 linear) { return linear > 1e-9f ? 6.0205999f * m_log2f(linear) : -180.0f; }

hz_internal void cas_muffle_apply(CasBusFx *b, f32 *lr, u32 frames) {
  const CasMuffle *m = b->muffle;
  if (m->lp_hz > 19900.0f && m->hs_db > -0.01f && m->gain > 0.9995f)
    return;
  if (m_absf(m->lp_hz - b->coef_hz) > b->coef_hz * 0.004f || m_absf(m->hs_db - b->coef_db) > 0.05f) {
    for (u32 ch = 0; ch < 2; ch++) {
      pg_svf_set(&b->mlp[ch], b->sr, PG_SVF_LOWPASS, m->lp_hz, pg_q_from_db(0.5f));
      cas_biquad_highshelf(&b->mhs[ch], b->sr, 3500.0f, m->hs_db);
    }
    b->coef_hz = m->lp_hz;
    b->coef_db = m->hs_db;
  }
  for (u32 i = 0; i < frames; i++) {
    for (u32 ch = 0; ch < 2; ch++) {
      f32 x = pg_svf_next(&b->mlp[ch], lr[2u * i + ch]);
      x = cas_biquad_next(&b->mhs[ch], x);
      lr[2u * i + ch] = x * m->gain;
    }
  }
}

hz_internal void cas_bus_effect(void *state, f32 *lr, u32 frames, u32 sample_rate) {
  CasBusFx *b = (CasBusFx *)state;
  const f32 k = 1.0f - m_expf(-1.0f / (m_maxf(b->duck_tau, 1e-4f) * (f32)sample_rate));
  for (u32 i = 0; i < frames; i++) {
    b->duck += (b->duck_target - b->duck) * k;
    const f32 g = b->duck * b->trim;
    lr[2u * i] *= g;
    lr[2u * i + 1u] *= g;
  }
  if (b->has_comp)
    pg_compressor_process(&b->comp, lr, frames);
  if (b->muffle)
    cas_muffle_apply(b, lr, frames);
  for (u32 i = 0; i < 2u * frames; i++) {
    b->meter_sq += (f64)lr[i] * (f64)lr[i];
    b->meter_peak = m_maxf(b->meter_peak, m_absf(lr[i]));
  }
  b->meter_frames += frames;
}

// the send bus: the convolver, then the same muffle as the world buses
typedef struct CasSendFx {
  CasConv *conv;
  CasBusFx tail; // only the muffle and the meter are used
} CasSendFx;

hz_internal void cas_send_effect(void *state, f32 *lr, u32 frames, u32 sample_rate) {
  CasSendFx *s = (CasSendFx *)state;
  cas_conv_effect(s->conv, lr, frames, sample_rate);
  cas_muffle_apply(&s->tail, lr, frames);
  for (u32 i = 0; i < 2u * frames; i++) {
    s->tail.meter_sq += (f64)lr[i] * (f64)lr[i];
    s->tail.meter_peak = m_maxf(s->tail.meter_peak, m_absf(lr[i]));
  }
  s->tail.meter_frames += frames;
}

// the master: tinnitus, the 0.22 headroom stage, the compressor, the soft clip and the volume
typedef struct CasMaster {
  PgCompressor comp;
  f32 gain, gain_target;
  f32 tin_gain, tin_target, tin_tau;
  f32 ph1, ph2, ph3, ph_lfo;
  f64 meter_sq;
  f32 meter_peak;
  u32 meter_frames;
} CasMaster;

hz_internal f32 cas_limiter(f32 x) {
  const f32 a = m_absf(x);
  const f32 y = a < 0.66f ? a : 0.66f + 0.34f * m_tanhf((a - 0.66f) / 0.34f);
  return (x < 0.0f ? -y : y) * 0.985f;
}

hz_internal void cas_master_effect(void *state, f32 *lr, u32 frames, u32 sample_rate) {
  CasMaster *m = (CasMaster *)state;
  const f32 sr = (f32)sample_rate;
  const b32 tin = m->tin_gain > 1e-6f || m->tin_target > 0.0f;
  const f32 tk = 1.0f - m_expf(-1.0f / (m_maxf(m->tin_tau, 1e-4f) * sr));
  for (u32 i = 0; i < frames; i++) {
    f32 t = 0.0f;
    if (tin) {
      m->tin_gain += (m->tin_target - m->tin_gain) * tk;
      m->ph1 += 3980.0f / sr;
      m->ph2 += (4130.0f + 26.0f * m_sinf(2.0f * M_PI * m->ph_lfo)) / sr;
      m->ph3 += 7420.0f / sr;
      m->ph_lfo += 0.23f / sr;
      m->ph1 -= m_floorf(m->ph1);
      m->ph2 -= m_floorf(m->ph2);
      m->ph3 -= m_floorf(m->ph3);
      m->ph_lfo -= m_floorf(m->ph_lfo);
      t = m->tin_gain * (0.6f * m_sinf(2.0f * M_PI * m->ph1) + 0.45f * m_sinf(2.0f * M_PI * m->ph2) + 0.18f * m_sinf(2.0f * M_PI * m->ph3));
    }
    lr[2u * i] = (lr[2u * i] + t) * 0.22f;
    lr[2u * i + 1u] = (lr[2u * i + 1u] + t) * 0.22f;
  }
  pg_compressor_process(&m->comp, lr, frames);
  const f32 vk = 1.0f - m_expf(-1.0f / (0.03f * sr));
  for (u32 i = 0; i < frames; i++) {
    m->gain += (m->gain_target - m->gain) * vk;
    const f32 l = cas_limiter(lr[2u * i]), r = cas_limiter(lr[2u * i + 1u]);
    m->meter_sq += (f64)(l * m->gain) * (f64)(l * m->gain) + (f64)(r * m->gain) * (f64)(r * m->gain);
    m->meter_peak = m_maxf(m->meter_peak, m_maxf(m_absf(l), m_absf(r)) * m->gain);
    const f32 out_l = l * m->gain / CAS_ENGINE_FINAL, out_r = r * m->gain / CAS_ENGINE_FINAL;
    lr[2u * i] = out_l;
    lr[2u * i + 1u] = out_r;
  }
  m->meter_frames += frames;
}

/* ------------------------------------------------------------ ducking and concussion (main lane, once a frame) */

typedef struct CasDuck {
  f32 amount, hold;
} CasDuck;

typedef enum CasDuckBus {
  CAS_DUCK_AMBIENCE = 0,
  CAS_DUCK_FOLEY,
  CAS_DUCK_VOICE,
  CAS_DUCK_COUNT,
} CasDuckBus;

// gunfire pushes the ambience, foley and voice down fast and lets them float back
hz_internal void cas_duck_hit(CasDuck *d, f32 *target, f32 *tau, u32 bus, f32 amount, f32 hold) {
  const f32 scale = bus == CAS_DUCK_AMBIENCE ? 1.0f : (bus == CAS_DUCK_FOLEY ? 0.55f : 0.4f);
  const f32 a = cas_clamp(amount * scale, 0.0f, 0.92f);
  if (a <= d->amount + 0.01f)
    return;
  d->amount = a;
  d->hold = hold;
  *target = 1.0f - a;
  *tau = 0.012f;
}

// hold, then the amount recovers at 2.6 per second
hz_internal void cas_duck_update(CasDuck *d, f32 *target, f32 *tau, f32 dt) {
  if (d->amount <= 0.0f)
    return;
  if (d->hold > 0.0f) {
    d->hold -= dt;
    return;
  }
  d->amount = m_maxf(0.0f, d->amount - dt * 2.6f);
  *target = 1.0f - d->amount;
  *tau = 0.09f;
  if (d->amount < 0.01f) {
    d->amount = 0.0f;
    *target = 1.0f;
  }
}
