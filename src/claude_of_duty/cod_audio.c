// Claude of Duty audio: synthesized clips rendered at load across the lanes, voices started from the packages' events.

// the Windows headers define these as empty or type macros; the synth names variables after them
#undef far
#undef near
#undef small

#include "cod_audio_syn.c"
#include "cod_audio_weapons.c"
#include "cod_audio_foley.c"
#include "cod_audio_vox.c"
#include "cod_audio_ambient.c"
#include "cod_audio_reverb.c"
#include "cod_audio_mix.c"
#include "cod_audio_bed.c"

#define CAS_SQRT2 1.41421356f // the engine's centre pan is -3 dB per channel; the panner and direct sends are not
#define CAS_SPEED_OF_SOUND 343.0f
#define CAS_SHOT_BUCKETS 8
#define CAS_EXP_BUCKETS 8
#define CAS_WHIZZ_COUNT 4
#define CAS_VOICE_PRESETS 3
#define CAS_FILTERS 3
#define CAS_MAX_CMDS 192
#define CAS_MAX_PENDING 96
#define CAS_MAX_TRACKS 128
#define CAS_MAX_SETS 400
#define CAS_JOB_BUDGET_MS 14.0 // wall time one lane spends on clip jobs in a frame
#define CAS_BED_AHEAD_S 0.15f
#define CAS_BED_VOLUME 0.5f // the ambience bus trim

// shot distance buckets (metres): a shot's waveform depends on its distance, so each bucket is rendered with its own mix
static const f32 CAS_SHOT_DIST[CAS_SHOT_BUCKETS] = {3, 8, 16, 30, 55, 95, 160, 260};
static const f32 CAS_EXP_DIST[CAS_EXP_BUCKETS] = {3, 10, 25, 50, 90, 150, 240, 330};
static const f32 CAS_WHIZZ_MISS[CAS_WHIZZ_COUNT] = {0.4f, 1.3f, 2.8f, 4.8f};
// the filtered clip levels: the lowpass and high shelf of the air absorption and occlusion at three strengths
static const f32 CAS_FILTER_LP[CAS_FILTERS] = {6500.0f, 2900.0f, 600.0f};
static const f32 CAS_FILTER_HS[CAS_FILTERS] = {-4.0f, -13.0f, -24.0f};

typedef enum CasKind {
  CAS_K_SHOT = 0,
  CAS_K_WHIZZ,
  CAS_K_DRYFIRE,
  CAS_K_IMPACT,
  CAS_K_STEP,
  CAS_K_CLOTH,
  CAS_K_SHELL,
  CAS_K_RELOAD,
  CAS_K_EXPLOSION,
  CAS_K_BODYFALL,
  CAS_K_UI,
  CAS_K_HEARTBEAT,
  CAS_K_BARK,
  CAS_K_AMBIENT,
  CAS_K_IR,
  CAS_K_COUNT,
} CasKind;

static const char *const CAS_KIND_NAME[CAS_K_COUNT] = {"shot",   "whizz", "dryfire", "impact", "step",    "cloth", "shell", "reload",
                                                       "explosion", "bodyfall", "ui", "heartbeat", "bark", "ambient", "ir"};

// a group of interchangeable clips: the variants rendered clear, and filtered copies of variant 0
typedef struct CasSet {
  u32 kind, p0, p1, p2;
  f32 dist;
  u32 variants;
  u32 filtered;
  u32 first;  // the first clip
  f32 send;   // the voice's reverb send
  u32 bus;
  u32 last;   // the variant played last, so a repeat is avoided
} CasSet;

typedef struct CasClip {
  HzHandleT(AudioSource) src;
  u32 frames;
  f32 level; // peak / 0.99: the gain that restores the rendered level
} CasClip;

typedef struct CasOut {
  i16 *pcm;
  u32 frames;
  f32 peak;
} CasOut;

typedef struct CasJob {
  u32 set, variant;
  u64 seed;
  f32 ms;
} CasJob;

// one voice to start: resolved by the consolidation systems, started by the main lane
typedef struct CasCmd {
  u32 clip;
  f32 volume; // before the duck of the ambience group
  f32 pitch;
  u32 bus;
  f32 send;   // the engine send, 0..1
  b32 positional;
  v3 pos;
  f32 pan;
  f32 delay;  // seconds until the voice starts
  b32 ambience;
} CasCmd;

typedef struct CasPending {
  CasCmd cmd;
  f32 left;
} CasPending;

typedef struct CasTrack {
  HzHandleT(AudioVoice) voice;
  v3 pos;
  b32 positional;
  b32 ambience;
  f32 base_volume;
} CasTrack;

typedef struct CodAudio {
  Allocator alloc;
  EcsWorld *world;
  AudioSystem system;
  u32 sr;

  CasSet *sets;
  u32 set_count;
  u32 base[CAS_K_COUNT];
  CasClip *clips;
  u32 clip_count;
  CasJob *jobs;
  u32 job_count;
  CasOut *outs; // four per job
  CasConv *conv;
  CasIrSpectra *ir_work;
  volatile u32 job_next, job_done;
  u32 lanes;
  ArenaAllocator *lane_arena[HZ_MAX_LANES];
  Allocator lane_alloc[HZ_MAX_LANES];
  u64 render_t0;
  b32 sources_made;

  CasBusFx bus[AUDIO_BUS_COUNT];
  CasSendFx send_fx;
  CasMaster master;
  CasMuffle muffle;
  f32 muffle_lp_target, muffle_hs_target, muffle_gain_target, muffle_tau;
  f32 deafness;
  CasDuck duck[CAS_DUCK_COUNT];
  f32 amb_duck, amb_duck_target, amb_duck_tau;
  f32 tin_until;

  CasBed bed;
  HzHandleT(AudioSource) bed_src;
  HzHandleT(AudioVoice) bed_voice;

  v3 listener_pos, listener_fwd, listener_up;
  CasSpaceWeights space;
  f32 probe_timer;
  v3 probe_pos;
  b32 probed;

  CodRng rng;
  f64 clock;      // mixer seconds
  u32 frame;
  FixedArray(CasCmd, CAS_MAX_CMDS) cmds;
  FixedArray(CasPending, CAS_MAX_PENDING) pending;
  FixedArray(CasTrack, CAS_MAX_TRACKS) tracks;
  u32 dropped;
  u32 plays;

  // per-frame budgets and rate limits
  u32 budget_impact, budget_step, budget_shell, budget_whizz;
  f64 last_bark, last_enemy_fire;
  f32 health;
  f32 heart_timer;
  u32 stance_seen;
  b32 ads_seen;
  b32 regen_seen;
  b32 first_stance;

  // the ambience scheduler
  f32 t_volley, t_boom, t_oneshot, t_chatter;

  b32 active;      // the mixer ticks this frame
  b32 suspended;
  f32 volume;      // CodSettings.volume
  f32 meter_timer;
  b32 meter;
} CodAudio;

/* ------------------------------------------------------------ sets and jobs */

hz_internal CasSet *cas_add_set(CodAudio *a, u32 kind, u32 p0, u32 p1, u32 p2, f32 dist, u32 variants, u32 filtered, f32 send, u32 bus) {
  assert_msg(a->set_count < CAS_MAX_SETS, "cod_audio: too many clip sets");
  CasSet *s = &a->sets[a->set_count++];
  *s = (CasSet){.kind = kind, .p0 = p0, .p1 = p1, .p2 = p2, .dist = dist, .variants = variants, .filtered = filtered, .send = send, .bus = bus};
  return s;
}

hz_internal const CasSet *cas_set_at(const CodAudio *a, u32 kind, u32 index) { return &a->sets[a->base[kind] + index]; }

hz_internal b32 cas_profile_used(u32 p) { return p == CAS_PROF_RIFLE || p == CAS_PROF_SMG || p == CAS_PROF_PISTOL; }
hz_internal b32 cas_profile_volley(u32 p) { return p == CAS_PROF_AK || p == CAS_PROF_LMG || p == CAS_PROF_SNIPER; }

hz_internal f32 cas_ambient_send(u32 kind) {
  switch (kind) {
  case CAS_AMB_DOG: return 0.7f;
  case CAS_AMB_SIREN: return 1.1f;
  case CAS_AMB_CREAK: return 0.8f;
  case CAS_AMB_SETTLE: return 0.6f;
  case CAS_AMB_BIRDS: return 0.9f;
  case CAS_AMB_VEHICLE: return 0.7f;
  case CAS_AMB_HELI: return 0.9f;
  default: return 1.2f;
  }
}

// the class distances an ambient kind is rendered at: near kinds (25 m, 70 m), far kinds (175 m)
hz_internal f32 cas_ambient_class_dist(u32 kind, u32 cls) { return cas_ambient_far(kind) ? 175.0f : (cls == 0 ? 25.0f : 70.0f); }

hz_internal void cas_build_sets(CodAudio *a) {
  a->sets = ALLOC_ARRAY(&a->alloc, CasSet, CAS_MAX_SETS);
  a->base[CAS_K_SHOT] = a->set_count;
  for (u32 p = 0; p < CAS_PROF_COUNT; p++) {
    const f32 send = CAS_PROFILES[p].send;
    cas_add_set(a, CAS_K_SHOT, p, 0, 0, 0.0f, cas_profile_used(p) ? 6u : 3u, 0, send, CAS_BUS_WEAPONS);
    for (u32 b = 0; b < CAS_SHOT_BUCKETS; b++) {
      u32 v = cas_profile_used(p) ? 2u : (cas_profile_volley(p) ? (b >= 5 ? 2u : 0u) : 1u);
      cas_add_set(a, CAS_K_SHOT, p, b + 1, 0, CAS_SHOT_DIST[b], v, cas_profile_used(p) ? CAS_FILTERS : 0u, send, CAS_BUS_WEAPONS);
    }
  }
  a->base[CAS_K_WHIZZ] = a->set_count;
  for (u32 m = 0; m < CAS_WHIZZ_COUNT; m++)
    cas_add_set(a, CAS_K_WHIZZ, m, 0, 0, CAS_WHIZZ_MISS[m], 2, 0, 0.25f, CAS_BUS_FOLEY);
  a->base[CAS_K_DRYFIRE] = a->set_count;
  cas_add_set(a, CAS_K_DRYFIRE, 0, 0, 0, 0.0f, 3, 0, 0.2f, CAS_BUS_WEAPONS);
  a->base[CAS_K_IMPACT] = a->set_count;
  for (u32 s = 0; s < CAS_SURFACES; s++)
    cas_add_set(a, CAS_K_IMPACT, s, 0, 0, 0.0f, 3, CAS_FILTERS, CAS_IMPACT[s].wet, CAS_BUS_FOLEY);
  a->base[CAS_K_STEP] = a->set_count;
  for (u32 s = 0; s < CAS_SURFACES; s++)
    for (u32 g = 0; g < CAS_GAIT_COUNT; g++)
      cas_add_set(a, CAS_K_STEP, s, g, 0, 0.0f, 2, 0, 0.3f, CAS_BUS_FOLEY);
  a->base[CAS_K_CLOTH] = a->set_count;
  cas_add_set(a, CAS_K_CLOTH, 0, 0, 0, 0.0f, 3, 0, 0.2f, CAS_BUS_FOLEY);
  a->base[CAS_K_SHELL] = a->set_count;
  for (u32 s = 0; s < CAS_SURFACES; s++)
    cas_add_set(a, CAS_K_SHELL, s, 0, 0, 0.0f, 3, 0, 0.42f, CAS_BUS_FOLEY);
  a->base[CAS_K_RELOAD] = a->set_count;
  for (u32 p = 0; p < 4; p++)
    cas_add_set(a, CAS_K_RELOAD, p, 0, 0, 0.0f, 3, 0, 0.3f, CAS_BUS_FOLEY);
  a->base[CAS_K_EXPLOSION] = a->set_count;
  for (u32 z = 0; z < CAS_EXPLOSION_SIZES; z++)
    for (u32 b = 0; b < CAS_EXP_BUCKETS; b++) {
      const u32 v = z == 0 ? 2u : (b >= 4 ? 1u : 0u);
      cas_add_set(a, CAS_K_EXPLOSION, z, b, 0, CAS_EXP_DIST[b], v, (z == 0 && b <= 4) ? CAS_FILTERS : 0u, 0.85f, CAS_BUS_WEAPONS);
    }
  a->base[CAS_K_BODYFALL] = a->set_count;
  cas_add_set(a, CAS_K_BODYFALL, 0, 0, 0, 0.0f, 3, CAS_FILTERS, 0.4f, CAS_BUS_FOLEY);
  a->base[CAS_K_UI] = a->set_count;
  for (u32 u = 0; u < CAS_UI_COUNT; u++)
    cas_add_set(a, CAS_K_UI, u, 0, 0, 0.0f, 1, 0, 0.0f, CAS_BUS_UI);
  a->base[CAS_K_HEARTBEAT] = a->set_count;
  cas_add_set(a, CAS_K_HEARTBEAT, 0, 0, 0, 0.0f, 2, 0, 0.1f, CAS_BUS_FOLEY);
  a->base[CAS_K_BARK] = a->set_count;
  for (u32 b = 0; b < CAS_BARK_COUNT; b++)
    for (u32 v = 0; v < CAS_VOICE_PRESETS; v++)
      for (u32 r = 0; r < 2; r++)
        cas_add_set(a, CAS_K_BARK, b, v, r, 0.0f, (r == 0 || v == 0) ? 1u : 0u, r == 0 ? CAS_FILTERS : 0u, r ? 0.05f : 0.45f, CAS_BUS_VOICE);
  a->base[CAS_K_AMBIENT] = a->set_count;
  for (u32 k = 0; k < CAS_AMB_COUNT; k++)
    for (u32 c = 0; c < 2; c++)
      cas_add_set(a, CAS_K_AMBIENT, k, c, 0, cas_ambient_class_dist(k, c), (cas_ambient_far(k) && c == 1) ? 0u : (cas_ambient_far(k) ? 1u : 2u), 0,
                  cas_ambient_send(k), CAS_BUS_WEAPONS);
  a->base[CAS_K_IR] = a->set_count;
  for (u32 s = 0; s < CAS_SPACES; s++)
    cas_add_set(a, CAS_K_IR, s, 0, 0, 0.0f, 1, 0, 0.0f, 0);
}

hz_internal u64 cas_seed(u32 set, u32 variant) {
  u64 z = 0xC0DA5EEDull + (u64)set * 0x9E3779B97F4A7C15ull + (u64)variant * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

hz_internal void cas_build_jobs(CodAudio *a) {
  u32 clips = 0, jobs = 0;
  for (u32 i = 0; i < a->set_count; i++) {
    CasSet *s = &a->sets[i];
    if (s->kind == CAS_K_IR) {
      jobs++;
      continue;
    }
    s->first = clips;
    clips += s->variants + s->filtered;
    jobs += s->variants;
  }
  a->clip_count = clips;
  a->clips = ALLOC_ARRAY(&a->alloc, CasClip, clips ? clips : 1u);
  a->job_count = jobs;
  a->jobs = ALLOC_ARRAY(&a->alloc, CasJob, jobs);
  a->outs = ALLOC_ARRAY(&a->alloc, CasOut, jobs * 4u);
  u32 j = 0;
  for (u32 i = 0; i < a->set_count; i++) {
    const CasSet *s = &a->sets[i];
    const u32 n = s->kind == CAS_K_IR ? 1u : s->variants;
    for (u32 v = 0; v < n; v++, j++)
      a->jobs[j] = (CasJob){.set = i, .variant = v, .seed = cas_seed(i, v)};
  }
}

/* ------------------------------------------------------------ the render (any lane) */

// peak-normalises a finished float render into PCM and keeps what the comparison needs
hz_internal void cas_finish_out(CodAudio *a, Allocator *lane_alloc, CasOut *out, f32 *x, u32 n) {
  f32 peak = 0.0f;
  for (u32 i = 0; i < n; i++) {
    if (!(x[i] == x[i]))
      x[i] = 0.0f;
    peak = m_maxf(peak, m_absf(x[i]));
  }
  u32 len = n;
  while (len > 1 && m_absf(x[len - 1]) < peak * 5e-4f)
    len--;
  len = MIN(n, len + 32u);
  const u32 fade = MIN(len, 192u);
  for (u32 i = 0; i < fade; i++) {
    const f32 t = (f32)(fade - 1u - i) / (f32)fade;
    x[len - fade + i] *= 0.5f - 0.5f * m_cosf(M_PI * t);
  }
  PgAudioStats st = pg_buf_measure(x, len, CAS_SR);
  out->frames = len;
  out->peak = st.peak;
  const f32 scale = peak > 1e-9f ? 0.99f / peak : 0.0f;
  out->pcm = ALLOC_ARRAY_NO_ZERO(lane_alloc, i16, len);
  for (u32 i = 0; i < len; i++)
    x[i] *= scale;
  pg_buf_to_pcm16(x, len, out->pcm);
  (void)a;
}

hz_internal CasSyn cas_render_voice(Allocator *al, const CasSet *set, u64 seed, u32 variant) {
  switch (set->kind) {
  case CAS_K_SHOT: {
    const u32 slot = variant + set->p1 * 3u;
    return cas_voice_shot(al, seed, set->p0, set->dist, set->p1 == 0, slot);
  }
  case CAS_K_WHIZZ: return cas_voice_whizz(al, seed, set->dist);
  case CAS_K_DRYFIRE: return cas_voice_dryfire(al, seed);
  case CAS_K_IMPACT: return cas_voice_impact(al, seed, set->p0);
  case CAS_K_STEP: return cas_voice_step(al, seed, set->p0, set->p1);
  case CAS_K_CLOTH: return cas_voice_cloth(al, seed);
  case CAS_K_SHELL: return cas_voice_shell(al, seed, set->p0);
  case CAS_K_RELOAD: return cas_voice_reload(al, seed, set->p0);
  case CAS_K_EXPLOSION: {
    CasSyn s = cas_voice_explosion(al, seed, CAS_EXPLOSION_RADIUS[set->p0], set->dist);
    cas_apply_filter(s.out, s.n, cas_air_cutoff(set->dist), 0.0f);
    return s;
  }
  case CAS_K_BODYFALL: return cas_voice_bodyfall(al, seed);
  case CAS_K_UI: return cas_voice_ui(al, seed, set->p0);
  case CAS_K_HEARTBEAT: return cas_voice_heartbeat(al, seed);
  case CAS_K_BARK: {
    static const f32 f0[CAS_VOICE_PRESETS] = {96.0f, 121.0f, 109.0f};
    static const f32 tract[CAS_VOICE_PRESETS] = {0.95f, 1.03f, 0.98f};
    return cas_voice_bark(al, seed, set->p0, f0[set->p1], tract[set->p1], set->p2 != 0);
  }
  default: return cas_voice_ambient(al, seed, set->p0, set->dist);
  }
}

hz_internal void cas_render_job(CodAudio *a, u32 job_index, u32 lane) {
  CasJob *job = &a->jobs[job_index];
  const CasSet *set = &a->sets[job->set];
  const u64 t0 = os_time_now();
  TempAllocator ta = tctx_temp_allocator_begin(NULL);
  if (set->kind == CAS_K_IR) {
    CasSyn s = cas_syn_begin(&ta.allocator, job->seed, 1);
    const u32 len = cas_ir_frames(set->p0, a->sr);
    f32 *left = ALLOC_ARRAY(&ta.allocator, f32, len);
    f32 *right = ALLOC_ARRAY(&ta.allocator, f32, len);
    cas_ir_generate(&s, set->p0, a->sr, left, right);
    f32 *wr = ALLOC_ARRAY(&ta.allocator, f32, CAS_CONV_FFT);
    f32 *wi = ALLOC_ARRAY(&ta.allocator, f32, CAS_CONV_FFT);
    cas_ir_spectra(&a->conv->ir[set->p0], &a->conv->fft, len, left, right, wr, wi);
  } else {
    CasSyn s = cas_render_voice(&ta.allocator, set, job->seed, job->variant);
    CasOut *outs = &a->outs[job_index * 4u];
    const u32 n_out = 1u + ((job->variant == 0) ? set->filtered : 0u);
    for (u32 o = 1; o < n_out; o++) {
      f32 *x = ALLOC_ARRAY(&ta.allocator, f32, s.n);
      mem_cpy(x, s.out, sizeof(f32) * s.n);
      cas_apply_filter(x, s.n, CAS_FILTER_LP[o - 1], CAS_FILTER_HS[o - 1]);
      cas_finish_out(a, &a->lane_alloc[lane], &outs[o], x, s.n);
    }
    cas_finish_out(a, &a->lane_alloc[lane], &outs[0], s.out, s.n);
  }
  tctx_temp_allocator_end(ta);
  job->ms = (f32)os_ticks_to_ms(os_time_diff(os_time_now(), t0));
}

// the clip render of this frame: every lane claims jobs until its budget is spent; nothing here needs a lane_sync
void cod_audio_lanes(EcsWorld *w) {
  CodAudio *a = ecs_resource_get(w, CodAudioState)->impl;
  if (!a || a->sources_made)
    return;
  const u32 lane = tctx_current()->thread_idx;
  const u64 t0 = os_time_now();
  for (;;) {
    const u32 claimed = (u32)atomic_u32_inc_eval(&a->job_next) - 1u;
    if (claimed >= a->job_count)
      break;
    cas_render_job(a, claimed, lane);
    atomic_u32_inc_eval(&a->job_done);
    if (os_ticks_to_ms(os_time_diff(os_time_now(), t0)) > CAS_JOB_BUDGET_MS)
      break;
  }
}

// main lane: the finished PCM becomes one AudioSource per clip; the lane arenas go
hz_internal void cas_make_sources(CodAudio *a, CodAudioState *st) {
  f64 cpu_ms = 0.0;
  f32 slowest = 0.0f;
  u32 slowest_job = 0;
  u64 bytes = 0;
  f64 kind_ms[CAS_K_COUNT] = {0};
  u32 kind_clips[CAS_K_COUNT] = {0};
  u64 kind_bytes[CAS_K_COUNT] = {0};
  for (u32 j = 0; j < a->job_count; j++) {
    const CasJob *job = &a->jobs[j];
    const CasSet *set = &a->sets[job->set];
    cpu_ms += job->ms;
    kind_ms[set->kind] += job->ms;
    if (job->ms > slowest) {
      slowest = job->ms;
      slowest_job = j;
    }
    if (set->kind == CAS_K_IR)
      continue;
    const u32 n_out = 1u + ((job->variant == 0) ? set->filtered : 0u);
    for (u32 o = 0; o < n_out; o++) {
      const CasOut *out = &a->outs[j * 4u + o];
      const u32 clip_index = set->first + (o == 0 ? job->variant : set->variants + o - 1u);
      CasClip *clip = &a->clips[clip_index];
      WavFile wav = create_wav_from_samples(out->pcm, out->frames, CAS_SR);
      clip->src = audio_source_from_wav(&a->system, &wav, &a->alloc);
      clip->frames = out->frames;
      clip->level = out->peak / 0.99f;
      bytes += (u64)out->frames * sizeof(i16);
      kind_clips[set->kind]++;
      kind_bytes[set->kind] += (u64)out->frames * sizeof(i16);
      if (!handle_is_valid(clip->src))
        log_error("cod_audio: no source for clip % of set %", fmt_u32(clip_index), fmt_cstr(CAS_KIND_NAME[set->kind]));
    }
  }
  for (u32 i = 0; i < a->lanes; i++) {
    arena_destroy(a->lane_arena[i]);
    a->lane_arena[i] = NULL;
  }
  a->sources_made = true;
  st->ready = true;
  st->progress = 1.0f;
  st->clips = a->clip_count;
  st->clip_mb = (f32)((f64)bytes / (1024.0 * 1024.0));
  st->render_ms = os_ticks_to_ms(os_time_diff(os_time_now(), a->render_t0));
  log_info("cod_audio: % clips (% MB of PCM) from % jobs rendered in % ms wall on % lanes, % ms of render work, slowest job % (% ms)",
           fmt_u32(a->clip_count), fmt_f32_p(st->clip_mb, 1), fmt_u32(a->job_count), fmt_f32_p((f32)st->render_ms, 0), fmt_u32(a->lanes),
           fmt_f32_p((f32)cpu_ms, 0), fmt_cstr(CAS_KIND_NAME[a->sets[a->jobs[slowest_job].set].kind]), fmt_f32_p(slowest, 1));
  for (u32 k = 0; k < CAS_K_COUNT; k++)
    if (kind_ms[k] > 0.0)
      log_info("cod_audio:   % % clips % MB % ms", fmt_cstr(CAS_KIND_NAME[k]), fmt_u32(kind_clips[k]),
               fmt_f32_p((f32)((f64)kind_bytes[k] / (1024.0 * 1024.0)), 2), fmt_f32_p((f32)kind_ms[k], 0));
}

#include "cod_audio_play.c"
#include "cod_audio_events.c"
#include "cod_audio_frame.c"
