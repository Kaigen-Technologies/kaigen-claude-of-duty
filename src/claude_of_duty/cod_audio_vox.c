// Claude of Duty audio, voice: formant synthesis of the enemy barks (glottal pulses, three formants, consonant bursts).

typedef enum CasVowel {
  CAS_V_A = 0,
  CAS_V_E,
  CAS_V_I,
  CAS_V_O,
  CAS_V_U,
  CAS_V_AH,
  CAS_V_EHR,
  CAS_V_OHH,
  CAS_V_COUNT,
} CasVowel;

// F1..F3 and their bandwidths, adult male, shouted register
typedef struct CasVowelSpec {
  f32 f1, f2, f3, bw1, bw2, bw3;
} CasVowelSpec;

static const CasVowelSpec CAS_VOWELS[CAS_V_COUNT] = {
    {730, 1090, 2440, 110, 130, 180}, {530, 1840, 2480, 90, 120, 170},  {300, 2290, 3010, 70, 130, 190},
    {570, 840, 2410, 90, 110, 170},   {325, 700, 2530, 70, 100, 170},   {640, 1200, 2500, 110, 140, 190},
    {490, 1350, 1690, 100, 130, 180}, {450, 900, 2300, 95, 115, 175},
};

typedef struct CasSyl {
  u32 vowel;
  f32 d, a, p;
  u32 onset; // 0 none, 'p' plosive, 'f' fricative, 'n' nasal
  f32 g;
} CasSyl;

typedef struct CasBark {
  const char *name;
  f32 f0, drive, breath, tremolo;
  b32 dying;
  FixedArray(CasSyl, 5) syl;
} CasBark;

typedef enum CasBarkId {
  CAS_BARK_CONTACT = 0,
  CAS_BARK_SPOTTED,
  CAS_BARK_RELOADING,
  CAS_BARK_GRENADE,
  CAS_BARK_FLANKING,
  CAS_BARK_SUPPRESSING,
  CAS_BARK_MOVEUP,
  CAS_BARK_HIT,
  CAS_BARK_PAIN,
  CAS_BARK_DEATH,
  CAS_BARK_COPY,
  CAS_BARK_COUNT,
} CasBarkId;

// the barks: each syllable is a vowel, duration, amplitude, pitch multiplier, onset consonant and the gap after it
static const CasBark CAS_BARKS[CAS_BARK_COUNT] = {
    {"contact", 1.18f, 1.25f, 0.16f, 0, false, {2, {{CAS_V_O, 0.13f, 1.0f, 1.06f, 'p', 0.012f}, {CAS_V_A, 0.19f, 1.0f, 1.16f, 'p', 0}}}},
    {"spotted", 1.1f, 1.1f, 0.16f, 0, false,
     {5, {{CAS_V_E, 0.1f, 0.9f, 1.05f, 0, 0.01f}, {CAS_V_A, 0.08f, 0.7f, 1.0f, 'n', 0.01f}, {CAS_V_I, 0.1f, 0.8f, 0.95f, 0, 0.06f},
          {CAS_V_A, 0.12f, 1.0f, 1.1f, 'f', 0.02f}, {CAS_V_E, 0.13f, 0.75f, 0.9f, 'p', 0}}}},
    {"reloading", 1.05f, 1.0f, 0.16f, 0, false,
     {3, {{CAS_V_I, 0.09f, 0.8f, 1.0f, 0, 0.01f}, {CAS_V_OHH, 0.16f, 1.0f, 1.12f, 0, 0.015f}, {CAS_V_I, 0.13f, 0.7f, 0.9f, 'p', 0}}}},
    {"grenade", 1.3f, 1.5f, 0.16f, 0, false, {2, {{CAS_V_E, 0.1f, 0.9f, 1.0f, 'p', 0.012f}, {CAS_V_A, 0.26f, 1.15f, 1.35f, 'n', 0}}}},
    {"flanking", 1.12f, 1.2f, 0.16f, 0, false, {2, {{CAS_V_A, 0.16f, 1.0f, 1.1f, 'f', 0.015f}, {CAS_V_I, 0.13f, 0.8f, 0.95f, 'n', 0}}}},
    {"suppressing", 1.08f, 1.15f, 0.16f, 0, false,
     {4, {{CAS_V_U, 0.09f, 0.75f, 0.98f, 'f', 0.01f}, {CAS_V_E, 0.14f, 1.0f, 1.12f, 'p', 0.02f}, {CAS_V_I, 0.1f, 0.7f, 0.9f, 0, 0.05f},
          {CAS_V_A, 0.18f, 0.95f, 1.05f, 'f', 0}}}},
    {"moveup", 1.1f, 1.2f, 0.16f, 0, false, {2, {{CAS_V_U, 0.16f, 1.0f, 1.08f, 'n', 0.03f}, {CAS_V_A, 0.14f, 0.9f, 1.0f, 0, 0}}}},
    {"hit", 1.25f, 1.6f, 0.5f, 0, false, {1, {{CAS_V_AH, 0.16f, 1.1f, 1.2f, 'p', 0}}}},
    {"pain", 1.15f, 1.3f, 0.65f, 14, false, {1, {{CAS_V_AH, 0.34f, 0.95f, 1.0f, 0, 0}}}},
    {"death", 1.05f, 1.4f, 1.0f, 22, true, {2, {{CAS_V_AH, 0.3f, 1.0f, 1.15f, 0, 0.02f}, {CAS_V_EHR, 0.42f, 0.6f, 0.62f, 0, 0}}}},
    {"copy", 1.0f, 0.9f, 0.16f, 0, false, {2, {{CAS_V_A, 0.1f, 0.85f, 1.0f, 'p', 0.02f}, {CAS_V_I, 0.12f, 0.7f, 0.88f, 'p', 0}}}},
};

#define CAS_VOX_LEAD 0.06f // the bark starts this far into the clip so the consonants can lead the first vowel

// the glottal wave: 39 harmonics at 1/n^1.15 with alternating phase, normalised to a peak of 1
hz_internal f32 *cas_glottal_table(CasSyn *s, u32 count) {
  f32 *t = ALLOC_ARRAY(s->al, f32, count + 1u);
  f32 peak = 1e-6f;
  for (u32 i = 0; i < count; i++) {
    const f32 x = (f32)i / (f32)count;
    f32 v = 0.0f;
    for (u32 n = 1; n < 40; n++)
      v += (1.0f / m_powf((f32)n, 1.15f)) * ((n & 1u) ? 1.0f : -0.75f) * m_sinf(2.0f * M_PI * (f32)n * x);
    t[i] = v;
    peak = m_maxf(peak, m_absf(v));
  }
  for (u32 i = 0; i < count; i++)
    t[i] /= peak;
  t[count] = t[0];
  return t;
}

hz_internal f32 cas_bark_seconds(const CasBark *b) {
  f32 total = 0.0f;
  for (u32 i = 0; i < b->syl.len; i++)
    total += b->syl.items[i].d + b->syl.items[i].g;
  return CAS_VOX_LEAD + total + (b->dying ? 0.75f : 0.0f) + 0.35f + 0.15f;
}

// the three formants glide between vowels, the pitch follows a per-syllable contour
hz_internal CasSyn cas_voice_bark(Allocator *al, u64 seed, u32 bark_id, f32 f0_base, f32 tract, b32 radio) {
  const CasBark *B = &CAS_BARKS[bark_id < CAS_BARK_COUNT ? bark_id : 0];
  CasSyn s = cas_syn_begin(al, seed, (u32)m_ceilf(cas_bark_seconds(B) * (f32)CAS_SR));
  const f32 t0 = CAS_VOX_LEAD;
  const f32 level = 1.0f;
  const f32 f0 = f0_base * B->f0;
  f32 total = 0.0f;
  for (u32 i = 0; i < B->syl.len; i++)
    total += B->syl.items[i].d + B->syl.items[i].g;

  CasParam freq = cas_param(&s, f0 * B->syl.items[0].p, 4u + 3u * B->syl.len);
  CasParam src_gain = cas_param(&s, 0.0f, 8u * B->syl.len);
  CasParam noise_gain = cas_param(&s, 0.0f, 5u * B->syl.len);
  CasParam *ff = ALLOC_ARRAY(s.al, CasParam, 3);
  CasParam *fq = ALLOC_ARRAY(s.al, CasParam, 3);
  const CasVowelSpec *first = &CAS_VOWELS[B->syl.items[0].vowel];
  for (u32 k = 0; k < 3; k++) {
    const f32 fk = (k == 0 ? first->f1 : (k == 1 ? first->f2 : first->f3)) * tract;
    const f32 bw = k == 0 ? first->bw1 : (k == 1 ? first->bw2 : first->bw3);
    ff[k] = cas_param(&s, fk, 3u * B->syl.len + 2u);
    fq[k] = cas_param(&s, cas_clamp(fk / bw, 1.5f, 14.0f), 2u * B->syl.len + 2u);
  }

  const f32 breath = B->breath * cas_range(&s, 0.8f, 1.25f);
  f32 t = t0;
  CasWin cons_sum = cas_win(&s, 0.0f, (f32)s.n / (f32)s.sr); // consonant bursts go straight to the output
  for (u32 i = 0; i < B->syl.len; i++) {
    const CasSyl *y = &B->syl.items[i];
    const CasVowelSpec *v = &CAS_VOWELS[y->vowel];
    const f32 amp = y->a * 0.5f;
    if (y->onset) {
      const f32 ct = m_maxf(t - (y->onset == 'f' ? 0.055f : 0.018f), 0.0f);
      const f32 rate = cas_range(&s, 0.9f, 1.3f);
      const f32 dur = y->onset == 'f' ? 0.12f : (y->onset == 'n' ? 0.08f : 0.05f);
      const f32 env = y->onset == 'f' ? 0.012f + 0.05f : (y->onset == 'n' ? 0.01f + 0.04f : 0.014f);
      CasWin w = cas_win(&s, ct, cas_src_len(dur, rate, env + 0.002f));
      cas_win_noise(&s, w, CAS_NOISE_WHITE, rate);
      if (y->onset == 'f')
        cas_win_svf_c(w, PG_SVF_BANDPASS, cas_range(&s, 3800.0f, 6500.0f), 1.1f);
      else
        cas_win_svf_c(w, PG_SVF_HIGHPASS, cas_range(&s, 1400.0f, 2600.0f), 0.7f);
      CasParam g = cas_param(&s, 0.0f, 5);
      if (y->onset == 'f') {
        cas_ad(&g, ct, 0.1f * level, 0.012f, 0.05f);
      } else if (y->onset == 'n') {
        cas_ad(&g, ct, 0.02f * level, 0.01f, 0.04f);
        cas_set(&ff[0], 260.0f * tract, ct);
      } else {
        cas_hit(&g, ct, 0.16f * level, 0.014f);
      }
      cas_win_gain(&s, w, &g);
      for (u32 j = 0; j < w.len; j++)
        cons_sum.x[w.a + j] += w.x[j];
    }
    const f32 gl = m_maxf(t - 0.03f, t0);
    for (u32 k = 0; k < 3; k++) {
      const f32 base = k == 0 ? v->f1 : (k == 1 ? v->f2 : v->f3);
      const f32 bw = k == 0 ? v->bw1 : (k == 1 ? v->bw2 : v->bw3);
      const f32 fk = base * tract * (1.0f + cas_range(&s, -0.02f, 0.02f));
      cas_target(&ff[k], fk, gl, 0.014f);
      cas_target(&fq[k], cas_clamp(fk / bw, 1.5f, 14.0f), gl, 0.02f);
    }
    const f32 p_target = f0 * y->p;
    cas_target(&freq, p_target, t, 0.03f);
    const b32 last = i + 1 == B->syl.len;
    if (B->dying && last)
      cas_sweep(&freq, t + 0.05f, p_target, p_target * 0.45f, y->d);
    else
      cas_target(&freq, p_target * 0.94f, t + y->d * 0.6f, 0.06f);
    const f32 rel = last ? (B->dying ? y->d * 0.9f : 0.055f) : 0.028f;
    cas_adsr(&src_gain, t, amp * level, 0.014f, y->d * 0.22f, y->d * 0.5f, 0.72f, rel);
    cas_ad(&noise_gain, t, amp * breath * level, 0.02f, y->d + rel);
    t += y->d + y->g;
  }

  CasWin exhale = {0};
  if (B->dying) {
    const f32 et = t + 0.05f;
    const f32 rate = cas_range(&s, 0.6f, 0.9f);
    exhale = cas_win(&s, et, cas_src_len(0.9f, rate, 0.08f + 0.6f + 0.002f));
    cas_win_noise(&s, exhale, CAS_NOISE_WHITE, rate);
    cas_win_svf_sweep(&s, exhale, PG_SVF_BANDPASS, et, 900.0f, 380.0f, 0.6f, 0.55f);
    CasParam eg = cas_param(&s, 0.0f, 5);
    cas_ad(&eg, et, 0.16f * level, 0.08f, 0.6f);
    cas_win_gain(&s, exhale, &eg);
    t = et + 0.7f;
  }
  const f32 end = t + 0.35f;
  const f32 src_start = m_maxf(t0 - 0.01f, 0.0f);
  CasWin ex = cas_win(&s, src_start, end - src_start);
  if (ex.len) {
    // the excitation: glottal pulses through the gain automation, plus aspiration noise
    f32 *table = cas_glottal_table(&s, 2048);
    f32 *fr = ALLOC_ARRAY(s.al, f32, ex.len);
    cas_param_fill(&freq, s.sr, s.n, ex.a, ex.len, fr);
    f32 phase = 0.0f;
    for (u32 i = 0; i < ex.len; i++) {
      phase += fr[i] / (f32)s.sr;
      phase -= m_floorf(phase);
      const f32 pos = phase * 2048.0f;
      const u32 k = (u32)pos;
      ex.x[i] = table[k] + (table[k + 1] - table[k]) * (pos - (f32)k);
    }
    cas_win_gain(&s, ex, &src_gain);
    const f32 nrate = cas_range(&s, 0.9f, 1.2f);
    CasWin nz = cas_win(&s, src_start, cas_src_len(end - src_start + 0.05f, nrate, end - src_start));
    cas_win_noise(&s, nz, CAS_NOISE_WHITE, nrate);
    cas_win_svf_c(nz, PG_SVF_BANDPASS, 1400.0f, 0.6f);
    CasParam ng = noise_gain;
    cas_win_gain(&s, nz, &ng);
    for (u32 i = 0; i < nz.len && i < ex.len; i++)
      ex.x[i] += nz.x[i];

    // three parallel formants, then the vocal tract's shaping
    CasWin mix = cas_win(&s, src_start, end - src_start);
    const f32 weights0 = 1.0f, weights1 = 0.55f, weights2 = 0.24f;
    for (u32 k = 0; k < 3; k++) {
      CasWin f = cas_win(&s, src_start, end - src_start);
      for (u32 i = 0; i < f.len; i++)
        f.x[i] = ex.x[i];
      cas_win_svf_fq(&s, f, PG_SVF_BANDPASS, &ff[k], &fq[k]);
      const f32 gk = k == 0 ? weights0 : (k == 1 ? weights1 : weights2);
      for (u32 i = 0; i < f.len; i++)
        mix.x[i] += f.x[i] * gk;
    }
    cas_win_peaking(mix, 480.0f, 1.1f, 4.0f);
    cas_win_peaking(mix, 2600.0f, 1.4f, 5.0f);
    cas_win_svf_c(mix, PG_SVF_HIGHPASS, 150.0f, 0.7f);
    cas_win_svf_c(mix, PG_SVF_LOWPASS, 5200.0f, 0.7f);
    cas_win_shape(&s, mix, cas_curve(&s, 1.6f * B->drive, 0.35f), 2);
    const f32 body = 1.5f * level;
    const f32 trem_hz = B->tremolo * cas_range(&s, 0.85f, 1.15f);
    for (u32 i = 0; i < mix.len; i++) {
      f32 g = body;
      const f32 tt = (f32)(mix.a + i) / (f32)s.sr;
      if (B->tremolo > 0.0f && tt >= t0 && tt < t0 + total + 0.4f)
        g += 0.35f * m_sinf(2.0f * M_PI * trem_hz * (tt - t0));
      mix.x[i] *= g;
    }
    cas_win_mix(&s, mix, 1.0f);
  }
  for (u32 i = 0; i < cons_sum.len; i++)
    s.out[cons_sum.a + i] += cons_sum.x[i];
  if (exhale.len)
    cas_win_mix(&s, exhale, 1.0f);
  for (u32 i = 0; i < s.n; i++)
    s.out[i] *= 0.2f; // the voice trim

  if (radio) {
    // squad comms: band-limited and driven, with a squelch click at both ends of the transmission
    CasWin all = {.a = 0, .len = s.n, .x = s.out};
    cas_win_svf_c(all, PG_SVF_HIGHPASS, 420.0f, 0.8f);
    cas_win_svf_c(all, PG_SVF_LOWPASS, 3200.0f, 0.9f);
    cas_win_shape(&s, all, cas_curve(&s, 7.0f, 0.3f), 2);
    cas_win_scale(all, 1.1f);
    for (u32 k = 0; k < 2; k++) {
      const f32 st = k == 0 ? m_maxf(t0 - 0.05f, 0.0f) : end - 0.2f;
      CasWin c = cas_win(&s, st, cas_src_len(0.06f, 1.1f, 0.03f + 0.002f));
      cas_win_noise(&s, c, CAS_NOISE_WHITE, 1.1f);
      cas_win_svf_c(c, PG_SVF_BANDPASS, 2600.0f, 1.6f);
      CasParam g = cas_param(&s, 0.0f, 3);
      cas_hit(&g, st, 0.09f, 0.03f);
      cas_win_gain(&s, c, &g);
      cas_win_mix(&s, c, 1.0f);
    }
  }
  return s;
}
