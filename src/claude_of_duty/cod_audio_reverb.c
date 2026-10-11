// Claude of Duty audio, spaces: impulse responses, the space classifier, and the blended partitioned convolution on the send.

#define CAS_SPACES 5
#define CAS_CONV_BLOCK 512u                    // frames per partition
#define CAS_CONV_FFT (2u * CAS_CONV_BLOCK)     // overlap-save transform size
#define CAS_CONV_BINS (CAS_CONV_FFT / 2u + 1u) // unique bins of a real transform
#define CAS_SEND_SCALE 3.0f                    // voice sends are the send / 3: the mixer clamps a send to 1, the sends go to 3

typedef enum CasSpace {
  CAS_SPACE_TIGHT = 0,
  CAS_SPACE_ROOM,
  CAS_SPACE_STREET,
  CAS_SPACE_TUNNEL,
  CAS_SPACE_OPEN,
} CasSpace;

typedef struct CasIrSpec {
  f32 seconds, rt60, predelay, hf_damp, bright, diffusion, width;
  FixedArray(f32, 9) taps;
  f32 tap_gain;
  u32 slaps;
  f32 slap_time;
} CasIrSpec;

// in CasSpace order
static const CasIrSpec CAS_IR_SPECS[CAS_SPACES] = {
    {0.5f, 0.34f, 0.0022f, 0.45f, 0.62f, 0.55f, 0.4f, {7, {0.0035f, 0.0061f, 0.0092f, 0.0134f, 0.0177f, 0.0231f, 0.0288f}}, 0.85f, 0, 0.0f},
    {1.5f, 1.05f, 0.006f, 0.55f, 0.5f, 0.8f, 0.6f, {8, {0.009f, 0.0143f, 0.021f, 0.0296f, 0.0381f, 0.0492f, 0.0613f, 0.078f}}, 0.6f, 0, 0.0f},
    {2.0f, 1.45f, 0.012f, 0.42f, 0.56f, 0.62f, 0.85f, {9, {0.017f, 0.029f, 0.046f, 0.063f, 0.088f, 0.112f, 0.147f, 0.19f, 0.24f}}, 0.72f, 7, 0.058f},
    {2.2f, 1.8f, 0.004f, 0.7f, 0.3f, 0.5f, 0.35f, {7, {0.006f, 0.012f, 0.019f, 0.027f, 0.037f, 0.049f, 0.064f}}, 0.9f, 12, 0.031f},
    {2.8f, 1.15f, 0.05f, 0.9f, 0.16f, 0.9f, 1.0f, {7, {0.07f, 0.115f, 0.18f, 0.26f, 0.35f, 0.48f, 0.62f}}, 0.3f, 0, 0.0f},
};

hz_internal u32 cas_ir_frames(u32 space, u32 sr) { return MAX(64u, (u32)m_floorf(CAS_IR_SPECS[space].seconds * (f32)sr)); }

hz_internal u32 cas_ir_parts(u32 space, u32 sr) { return (cas_ir_frames(space, sr) + CAS_CONV_BLOCK - 1u) / CAS_CONV_BLOCK; }

/* ------------------------------------------------------------ FFT */

typedef struct CasFft {
  u32 n;
  f32 *cos_t, *sin_t; // n/2 twiddles
  u32 *rev;
} CasFft;

hz_internal CasFft cas_fft_make(Allocator *al, u32 n) {
  CasFft f = {.n = n, .cos_t = ALLOC_ARRAY(al, f32, n / 2u), .sin_t = ALLOC_ARRAY(al, f32, n / 2u), .rev = ALLOC_ARRAY(al, u32, n)};
  for (u32 i = 0; i < n / 2u; i++) {
    f.cos_t[i] = (f32)m_cos(2.0 * M_PI * (f64)i / (f64)n);
    f.sin_t[i] = (f32)m_sin(2.0 * M_PI * (f64)i / (f64)n);
  }
  u32 bits = 0;
  while ((1u << bits) < n)
    bits++;
  for (u32 i = 0; i < n; i++) {
    u32 r = 0;
    for (u32 b = 0; b < bits; b++)
      r |= ((i >> b) & 1u) << (bits - 1u - b);
    f.rev[i] = r;
  }
  return f;
}

// in place radix-2; the inverse is unscaled
hz_internal void cas_fft(const CasFft *f, f32 *re, f32 *im, b32 inverse) {
  const u32 n = f->n;
  for (u32 i = 0; i < n; i++) {
    const u32 j = f->rev[i];
    if (j > i) {
      const f32 tr = re[i], ti = im[i];
      re[i] = re[j];
      im[i] = im[j];
      re[j] = tr;
      im[j] = ti;
    }
  }
  const f32 sign = inverse ? 1.0f : -1.0f;
  for (u32 len = 2; len <= n; len <<= 1) {
    const u32 half = len >> 1, step = n / len;
    for (u32 i = 0; i < n; i += len) {
      for (u32 k = 0; k < half; k++) {
        const f32 wr = f->cos_t[k * step], wi = sign * f->sin_t[k * step];
        const u32 a = i + k, b = a + half;
        const f32 xr = re[b] * wr - im[b] * wi;
        const f32 xi = re[b] * wi + im[b] * wr;
        re[b] = re[a] - xr;
        im[b] = im[a] - xi;
        re[a] += xr;
        im[a] += xi;
      }
    }
  }
}

/* ------------------------------------------------------------ impulse responses */

// stereo, decorrelated, peak-normalised to 0.42 across both channels
hz_internal void cas_ir_generate(CasSyn *s, u32 space, u32 sr, f32 *left, f32 *right) {
  const CasIrSpec *sp = &CAS_IR_SPECS[space];
  const u32 len = cas_ir_frames(space, sr);
  for (u32 ch = 0; ch < 2; ch++) {
    f32 *d = ch == 0 ? left : right;
    const f32 wob = 1.0f + (ch == 0 ? -1.0f : 1.0f) * sp->width * 0.06f;
    f32 lp1 = 0.0f, lp2 = 0.0f, hp = 0.0f, hp_prev = 0.0f;
    const f32 decay_k = m_logf(1e-3f) / (sp->rt60 * (f32)sr);
    for (u32 i = 0; i < len; i++) {
      const f32 t = (f32)i / (f32)sr;
      const f32 build = cas_clamp((t - sp->predelay) / (0.004f + sp->diffusion * 0.05f), 0.0f, 1.0f);
      const f32 env = m_expf(decay_k * (f32)i) * build * build;
      if (env < 1e-6f) {
        d[i] = 0.0f;
        continue;
      }
      f32 x = cod_rng_signed(&s->rng);
      if (cod_rng_float(&s->rng) > 0.25f + 0.75f * cas_clamp(t / (sp->rt60 * 0.5f), 0.0f, 1.0f))
        x *= 0.25f;
      const f32 norm = cas_clamp(t / sp->rt60, 0.0f, 1.4f);
      const f32 a = cas_clamp(0.9f * (1.0f - sp->hf_damp * norm) * sp->bright + 0.06f, 0.02f, 0.95f);
      lp1 += a * (x - lp1);
      lp2 += a * (lp1 - lp2);
      hp = lp2 - hp_prev + 0.995f * hp;
      hp_prev = lp2;
      d[i] = hp * env * sp->diffusion;
    }
    for (u32 k = 0; k < sp->taps.len; k++) {
      const f32 tt = (sp->predelay + sp->taps.items[k]) * wob * cas_range(s, 0.97f, 1.03f);
      const u32 idx = (u32)m_floorf(tt * (f32)sr);
      if (idx + 8u >= len)
        continue;
      const f32 g = (sp->tap_gain / (1.0f + (f32)k * 0.55f)) * cas_range(s, 0.65f, 1.0f) * (cas_unit(s) < 0.5f ? -1.0f : 1.0f);
      const u32 smear = 3u + (u32)m_floorf(sp->diffusion * 22.0f);
      f32 acc = 0.0f;
      for (u32 j = 0; j < smear && idx + j < len; j++) {
        acc = acc * 0.6f + cod_rng_signed(&s->rng) * 0.4f;
        const f32 w = 1.0f - (f32)j / (f32)smear;
        d[idx + j] += g * (j == 0 ? 1.0f : acc * w * w);
      }
    }
    for (u32 k = 1; k <= sp->slaps; k++) {
      const f32 tt = (sp->predelay + sp->slap_time * (f32)k * cas_range(s, 0.985f, 1.015f)) * wob;
      const u32 idx = (u32)m_floorf(tt * (f32)sr);
      if (idx + 64u >= len)
        continue;
      const f32 g = 0.55f * m_powf(0.68f, (f32)k) * (cas_unit(s) < 0.5f ? -1.0f : 1.0f);
      f32 s1 = 0.0f, s2 = 0.0f;
      for (u32 j = 0; j < 400 && idx + j < len; j++) {
        const f32 x = cod_rng_signed(&s->rng) * m_expf(-(f32)j / 90.0f);
        s1 += 0.35f * (x - s1);
        s2 += 0.35f * (s1 - s2);
        d[idx + j] += g * s2 * 3.2f;
      }
    }
  }
  f32 peak = 1e-9f;
  for (u32 i = 0; i < len; i++)
    peak = m_maxf(peak, m_maxf(m_absf(left[i]), m_absf(right[i])));
  const f32 g = 0.42f / peak;
  for (u32 i = 0; i < len; i++) {
    left[i] *= g;
    right[i] *= g;
  }
}

/* ------------------------------------------------------------ the convolver */

// one space's partitioned spectra: [part][bin] per channel
typedef struct CasIrSpectra {
  u32 parts;
  f32 *re[2];
  f32 *im[2];
} CasIrSpectra;

typedef struct CasConv {
  u32 sr;
  CasFft fft;
  CasIrSpectra ir[CAS_SPACES];
  u32 fdl_parts, fdl_pos;
  f32 *fdl_re[2];
  f32 *fdl_im[2]; // [slot][bin]
  f32 *blk[2];    // the input block, two lengths of overlap-save history
  f32 *out[2];    // the last computed block's output, read out over the next block
  f32 *work_re, *work_im;
  f32 *acc_re[2], *acc_im[2];
  f32 *y_re[2], *y_im[2];
  u32 fill;
  f32 weight[CAS_SPACES], target[CAS_SPACES];
  PgSvf hp[2], lp[2]; // the pre-send shaping: no sub into the convolvers, no fizz above 9 kHz
  f32 gain;           // the reverb return (0.9) times the send scale
  f32 smooth_k;       // the per-block weight easing, tau 0.4 s
  f32 smooth_tau;
  u32 live;           // spaces convolved last block
} CasConv;

hz_internal CasConv *cas_conv_make(Allocator *al, u32 sr) {
  CasConv *c = ALLOC(al, CasConv);
  c->sr = sr;
  c->fft = cas_fft_make(al, CAS_CONV_FFT);
  u32 max_parts = 0;
  for (u32 s = 0; s < CAS_SPACES; s++) {
    CasIrSpectra *ir = &c->ir[s];
    ir->parts = cas_ir_parts(s, sr);
    max_parts = MAX(max_parts, ir->parts);
    for (u32 ch = 0; ch < 2; ch++) {
      ir->re[ch] = ALLOC_ARRAY(al, f32, ir->parts * CAS_CONV_BINS);
      ir->im[ch] = ALLOC_ARRAY(al, f32, ir->parts * CAS_CONV_BINS);
    }
  }
  c->fdl_parts = max_parts;
  for (u32 ch = 0; ch < 2; ch++) {
    c->fdl_re[ch] = ALLOC_ARRAY(al, f32, max_parts * CAS_CONV_BINS);
    c->fdl_im[ch] = ALLOC_ARRAY(al, f32, max_parts * CAS_CONV_BINS);
    c->blk[ch] = ALLOC_ARRAY(al, f32, CAS_CONV_FFT);
    c->out[ch] = ALLOC_ARRAY(al, f32, CAS_CONV_BLOCK);
    c->acc_re[ch] = ALLOC_ARRAY(al, f32, CAS_CONV_BINS);
    c->acc_im[ch] = ALLOC_ARRAY(al, f32, CAS_CONV_BINS);
    c->y_re[ch] = ALLOC_ARRAY(al, f32, CAS_CONV_BINS);
    c->y_im[ch] = ALLOC_ARRAY(al, f32, CAS_CONV_BINS);
    pg_svf_set(&c->hp[ch], sr, PG_SVF_HIGHPASS, 170.0f, pg_q_from_db(0.7f));
    pg_svf_set(&c->lp[ch], sr, PG_SVF_LOWPASS, 9000.0f, pg_q_from_db(0.7f));
  }
  c->work_re = ALLOC_ARRAY(al, f32, CAS_CONV_FFT);
  c->work_im = ALLOC_ARRAY(al, f32, CAS_CONV_FFT);
  c->gain = 0.9f * CAS_SEND_SCALE;
  c->smooth_tau = 0.4f;
  c->smooth_k = 1.0f - m_expf(-(f32)CAS_CONV_BLOCK / ((f32)sr * c->smooth_tau));
  c->weight[CAS_SPACE_STREET] = c->target[CAS_SPACE_STREET] = 0.35f;
  c->weight[CAS_SPACE_OPEN] = c->target[CAS_SPACE_OPEN] = 0.65f;
  return c;
}

// the spectra of one space's two channels: each channel cut into blocks, zero-padded to the transform size
hz_internal void cas_ir_spectra(CasIrSpectra *ir, const CasFft *fft, u32 len, const f32 *left, const f32 *right, f32 *work_re, f32 *work_im) {
  for (u32 p = 0; p < ir->parts; p++) {
    for (u32 i = 0; i < CAS_CONV_FFT; i++) {
      const u32 at = p * CAS_CONV_BLOCK + i;
      work_re[i] = (i < CAS_CONV_BLOCK && at < len) ? left[at] : 0.0f;
      work_im[i] = (i < CAS_CONV_BLOCK && at < len) ? right[at] : 0.0f;
    }
    cas_fft(fft, work_re, work_im, false);
    // z = L + iR: L[k] = (z[k] + conj z[N-k]) / 2, R[k] = (z[k] - conj z[N-k]) / 2i
    for (u32 k = 0; k < CAS_CONV_BINS; k++) {
      const u32 m = (CAS_CONV_FFT - k) & (CAS_CONV_FFT - 1u);
      const f32 zr = work_re[k], zi = work_im[k], cr = work_re[m], ci = -work_im[m];
      const u32 o = p * CAS_CONV_BINS + k;
      ir->re[0][o] = 0.5f * (zr + cr);
      ir->im[0][o] = 0.5f * (zi + ci);
      ir->re[1][o] = 0.5f * (zi - ci);
      ir->im[1][o] = -0.5f * (zr - cr);
    }
  }
}

// one block: transform the newest 2B frames, push the spectrum, multiply-accumulate each live space's partitions, inverse
hz_internal void cas_conv_block(CasConv *c) {
  for (u32 i = 0; i < CAS_CONV_FFT; i++) {
    c->work_re[i] = c->blk[0][i];
    c->work_im[i] = c->blk[1][i];
  }
  cas_fft(&c->fft, c->work_re, c->work_im, false);
  c->fdl_pos = (c->fdl_pos + 1u) % c->fdl_parts;
  f32 *const sre[2] = {c->fdl_re[0] + c->fdl_pos * CAS_CONV_BINS, c->fdl_re[1] + c->fdl_pos * CAS_CONV_BINS};
  f32 *const sim[2] = {c->fdl_im[0] + c->fdl_pos * CAS_CONV_BINS, c->fdl_im[1] + c->fdl_pos * CAS_CONV_BINS};
  for (u32 k = 0; k < CAS_CONV_BINS; k++) {
    const u32 m = (CAS_CONV_FFT - k) & (CAS_CONV_FFT - 1u);
    const f32 zr = c->work_re[k], zi = c->work_im[k], cr = c->work_re[m], ci = -c->work_im[m];
    sre[0][k] = 0.5f * (zr + cr);
    sim[0][k] = 0.5f * (zi + ci);
    sre[1][k] = 0.5f * (zi - ci);
    sim[1][k] = -0.5f * (zr - cr);
  }

  for (u32 ch = 0; ch < 2; ch++) {
    mem_zero(c->y_re[ch], sizeof(f32) * CAS_CONV_BINS);
    mem_zero(c->y_im[ch], sizeof(f32) * CAS_CONV_BINS);
  }
  u32 live = 0;
  for (u32 s = 0; s < CAS_SPACES; s++) {
    c->weight[s] += (c->target[s] - c->weight[s]) * c->smooth_k;
    if (c->weight[s] <= 0.012f && c->target[s] <= 0.012f)
      continue; // a convolver with no weight is not computed
    live++;
    const CasIrSpectra *ir = &c->ir[s];
    for (u32 ch = 0; ch < 2; ch++) {
      f32 *ar = c->acc_re[ch], *ai = c->acc_im[ch];
      mem_zero(ar, sizeof(f32) * CAS_CONV_BINS);
      mem_zero(ai, sizeof(f32) * CAS_CONV_BINS);
      for (u32 p = 0; p < ir->parts; p++) {
        const u32 slot = (c->fdl_pos + c->fdl_parts - p) % c->fdl_parts;
        const f32 *xr = c->fdl_re[ch] + slot * CAS_CONV_BINS, *xi = c->fdl_im[ch] + slot * CAS_CONV_BINS;
        const f32 *hr = ir->re[ch] + p * CAS_CONV_BINS, *hi = ir->im[ch] + p * CAS_CONV_BINS;
        for (u32 k = 0; k < CAS_CONV_BINS; k++) {
          ar[k] += xr[k] * hr[k] - xi[k] * hi[k];
          ai[k] += xr[k] * hi[k] + xi[k] * hr[k];
        }
      }
      const f32 w = c->weight[s];
      for (u32 k = 0; k < CAS_CONV_BINS; k++) {
        c->y_re[ch][k] += w * ar[k];
        c->y_im[ch][k] += w * ai[k];
      }
    }
  }
  c->live = live;

  // z = L + iR from the two half spectra, then the inverse; the last B frames are the valid output
  for (u32 k = 0; k < CAS_CONV_BINS; k++) {
    c->work_re[k] = c->y_re[0][k] - c->y_im[1][k];
    c->work_im[k] = c->y_im[0][k] + c->y_re[1][k];
    if (k > 0 && k < CAS_CONV_FFT / 2u) {
      const u32 m = CAS_CONV_FFT - k;
      c->work_re[m] = c->y_re[0][k] + c->y_im[1][k];
      c->work_im[m] = -c->y_im[0][k] + c->y_re[1][k];
    }
  }
  cas_fft(&c->fft, c->work_re, c->work_im, true);
  const f32 inv = c->gain / (f32)CAS_CONV_FFT;
  for (u32 i = 0; i < CAS_CONV_BLOCK; i++) {
    c->out[0][i] = c->work_re[CAS_CONV_BLOCK + i] * inv;
    c->out[1][i] = c->work_im[CAS_CONV_BLOCK + i] * inv;
  }
}

// the send bus effect: fully wet, one block behind the input
hz_internal void cas_conv_effect(void *state, f32 *lr, u32 frames, u32 sample_rate) {
  CasConv *c = (CasConv *)state;
  (void)sample_rate;
  for (u32 i = 0; i < frames; i++) {
    const f32 in_l = pg_svf_next(&c->lp[0], pg_svf_next(&c->hp[0], lr[2u * i]));
    const f32 in_r = pg_svf_next(&c->lp[1], pg_svf_next(&c->hp[1], lr[2u * i + 1u]));
    c->blk[0][CAS_CONV_BLOCK + c->fill] = in_l;
    c->blk[1][CAS_CONV_BLOCK + c->fill] = in_r;
    lr[2u * i] = c->out[0][c->fill];
    lr[2u * i + 1u] = c->out[1][c->fill];
    if (++c->fill == CAS_CONV_BLOCK) {
      cas_conv_block(c);
      for (u32 j = 0; j < CAS_CONV_BLOCK; j++) {
        c->blk[0][j] = c->blk[0][CAS_CONV_BLOCK + j];
        c->blk[1][j] = c->blk[1][CAS_CONV_BLOCK + j];
      }
      c->fill = 0;
    }
  }
  for (u32 ch = 0; ch < 2; ch++) {
    c->hp[ch].ic1eq = (m_absf(c->hp[ch].ic1eq) < 1e-20f) ? 0.0f : c->hp[ch].ic1eq;
    c->hp[ch].ic2eq = (m_absf(c->hp[ch].ic2eq) < 1e-20f) ? 0.0f : c->hp[ch].ic2eq;
    c->lp[ch].ic1eq = (m_absf(c->lp[ch].ic1eq) < 1e-20f) ? 0.0f : c->lp[ch].ic1eq;
    c->lp[ch].ic2eq = (m_absf(c->lp[ch].ic2eq) < 1e-20f) ? 0.0f : c->lp[ch].ic2eq;
  }
}

/* ------------------------------------------------------------ the space classifier */

typedef struct CasSpaceWeights {
  f32 w[CAS_SPACES];
  f32 enclosure, mean_free, ceiling, close_sides, median;
} CasSpaceWeights;

#define CAS_PROBE_RAYS 9
#define CAS_PROBE_DIST 40.0f

// from 8 horizon rays and one up
hz_internal CasSpaceWeights cas_classify_space(const f32 *hits) {
  const u32 horiz = CAS_PROBE_RAYS - 1u;
  f32 sum = 0.0f, min_d = CAS_PROBE_DIST, max_d = 0.0f;
  u32 close = 0;
  FixedArray(f32, 16) sorted = {0};
  for (u32 i = 0; i < horiz; i++) {
    const f32 d = m_minf(hits[i], CAS_PROBE_DIST);
    sum += d;
    if (d < 12.0f)
      close++;
    min_d = m_minf(min_d, d);
    max_d = m_maxf(max_d, d);
    sorted.items[i] = d;
  }
  sorted.len = horiz;
  const f32 ceil_d = m_minf(hits[horiz], CAS_PROBE_DIST);
  const f32 close_sides = (f32)close / (f32)horiz;
  for (u32 i = 1; i < horiz; i++) {
    const f32 v = sorted.items[i];
    u32 j = i;
    while (j > 0 && sorted.items[j - 1] > v) {
      sorted.items[j] = sorted.items[j - 1];
      j--;
    }
    sorted.items[j] = v;
  }
  const f32 median = (horiz & 1u) ? sorted.items[horiz >> 1] : (sorted.items[horiz / 2u - 1u] + sorted.items[horiz / 2u]) * 0.5f;
  const f32 roofed = 1.0f - cas_clamp((ceil_d - 2.8f) / 7.0f, 0.0f, 1.0f);
  const f32 elong = cas_clamp((max_d - min_d) / m_maxf(max_d, 1.0f), 0.0f, 1.0f);
  const f32 small = cas_clamp(1.0f - median / 9.0f, 0.0f, 1.0f);
  const f32 indoor = roofed, outdoor = 1.0f - indoor;
  const f32 tunnel = indoor * elong * elong * 0.55f;
  const f32 rest = m_maxf(indoor - tunnel, 0.0f);
  CasSpaceWeights r = {0};
  r.w[CAS_SPACE_TUNNEL] = tunnel;
  r.w[CAS_SPACE_TIGHT] = rest * small;
  r.w[CAS_SPACE_ROOM] = rest * (1.0f - small);
  r.w[CAS_SPACE_STREET] = outdoor * cas_clamp(close_sides * 2.2f, 0.0f, 1.0f);
  r.w[CAS_SPACE_OPEN] = outdoor * cas_clamp(1.0f - close_sides * 1.8f, 0.06f, 1.0f);
  f32 tot = 0.0f;
  for (u32 k = 0; k < CAS_SPACES; k++)
    tot += r.w[k];
  if (tot < 1e-4f) {
    r.w[CAS_SPACE_OPEN] = 1.0f;
    tot = 1.0f;
  }
  for (u32 k = 0; k < CAS_SPACES; k++)
    r.w[k] /= tot;
  r.enclosure = cas_clamp(roofed * (0.45f + 0.55f * close_sides), 0.0f, 1.0f);
  r.mean_free = sum / (f32)horiz;
  r.ceiling = ceil_d;
  r.close_sides = close_sides;
  r.median = median;
  return r;
}
