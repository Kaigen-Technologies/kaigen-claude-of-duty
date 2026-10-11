// Claude of Duty audio, init and the main lane's frame: the device and mixer, the clip sources, the commands becoming voices, the bus meters.


hz_internal void cas_mixer_setup(CodAudio *a) {
  a->muffle = (CasMuffle){.lp_hz = 20000.0f, .hs_db = 0.0f, .gain = 1.0f};
  a->muffle_lp_target = 20000.0f;
  a->muffle_gain_target = 1.0f;
  a->muffle_tau = 0.25f;
  a->amb_duck = a->amb_duck_target = 1.0f;
  a->amb_duck_tau = 0.09f;
  const u32 sr = (u32)a->system.output_sample_rate;

  // trim and compressor per bus, the ambience rides the SFX bus with the weapons
  CasBusFx *w = &a->bus[CAS_BUS_WEAPONS], *f = &a->bus[CAS_BUS_FOLEY], *v = &a->bus[CAS_BUS_VOICE], *u = &a->bus[CAS_BUS_UI];
  *w = (CasBusFx){.sr = sr, .trim = 0.95f, .duck = 1.0f, .duck_target = 1.0f, .duck_tau = 0.09f, .has_comp = true, .muffle = &a->muffle};
  *f = (CasBusFx){.sr = sr, .trim = 0.9f, .duck = 1.0f, .duck_target = 1.0f, .duck_tau = 0.09f, .has_comp = true, .muffle = &a->muffle};
  *v = (CasBusFx){.sr = sr, .trim = 0.85f, .duck = 1.0f, .duck_target = 1.0f, .duck_tau = 0.09f, .has_comp = true, .muffle = &a->muffle};
  *u = (CasBusFx){.sr = sr, .trim = 1.6f, .duck = 1.0f, .duck_target = 1.0f, .duck_tau = 0.09f, .has_comp = false, .muffle = NULL};
  const PgCompressorDesc cw = {-7.0f, 8.0f, 2.6f, 0.003f, 0.16f};
  const PgCompressorDesc cf = {-14.0f, 10.0f, 2.0f, 0.004f, 0.2f};
  const PgCompressorDesc cv = {-18.0f, 8.0f, 3.0f, 0.006f, 0.22f};
  pg_compressor_init(&w->comp, sr, &cw, ALLOC_ARRAY(&a->alloc, f32, PG_COMPRESSOR_STORAGE));
  pg_compressor_init(&f->comp, sr, &cf, ALLOC_ARRAY(&a->alloc, f32, PG_COMPRESSOR_STORAGE));
  pg_compressor_init(&v->comp, sr, &cv, ALLOC_ARRAY(&a->alloc, f32, PG_COMPRESSOR_STORAGE));
  for (u32 i = 0; i < 3; i++) {
    CasBusFx *b = i == 0 ? w : (i == 1 ? f : v);
    for (u32 ch = 0; ch < 2; ch++) {
      pg_svf_set(&b->mlp[ch], sr, PG_SVF_LOWPASS, 20000.0f, pg_q_from_db(0.5f));
      cas_biquad_highshelf(&b->mhs[ch], sr, 3500.0f, 0.0f);
    }
    b->coef_hz = 20000.0f;
  }
  audio_bus_set_effect(&a->system, CAS_BUS_WEAPONS, cas_bus_effect, w);
  audio_bus_set_effect(&a->system, CAS_BUS_FOLEY, cas_bus_effect, f);
  audio_bus_set_effect(&a->system, CAS_BUS_VOICE, cas_bus_effect, v);
  audio_bus_set_effect(&a->system, CAS_BUS_UI, cas_bus_effect, u);

  a->conv = cas_conv_make(&a->alloc, sr);
  a->send_fx = (CasSendFx){.conv = a->conv, .tail = (CasBusFx){.sr = sr, .muffle = &a->muffle}};
  for (u32 ch = 0; ch < 2; ch++) {
    pg_svf_set(&a->send_fx.tail.mlp[ch], sr, PG_SVF_LOWPASS, 20000.0f, pg_q_from_db(0.5f));
    cas_biquad_highshelf(&a->send_fx.tail.mhs[ch], sr, 3500.0f, 0.0f);
  }
  a->send_fx.tail.coef_hz = 20000.0f;
  audio_send_set_effect(&a->system, cas_send_effect, &a->send_fx);

  const PgCompressorDesc cm = {-2.0f, 3.0f, 4.0f, 0.0035f, 0.14f};
  pg_compressor_init(&a->master.comp, sr, &cm, ALLOC_ARRAY(&a->alloc, f32, PG_COMPRESSOR_STORAGE));
  a->master.gain = 0.0f;
  a->master.gain_target = CAS_MASTER_VOLUME;
  a->master.tin_tau = 0.03f;
  audio_bus_set_effect(&a->system, AUDIO_BUS_MASTER, cas_master_effect, &a->master);
}

void cod_audio_init(EcsWorld *w) {
  Allocator alloc = make_arena_allocator(arena_create(MB(768), KB(256)));
  CodAudio *a = ALLOC(&alloc, CodAudio);
  a->alloc = alloc;
  a->world = w;
  a->sr = CAS_SR;
  a->rng = cod_rng_make(0xA0D10u);
  a->volume = 1.0f;
  a->lanes = tctx_current()->thread_count;
  for (u32 i = 0; i < a->lanes; i++) {
    a->lane_arena[i] = arena_create(MB(96), KB(256));
    a->lane_alloc[i] = make_arena_allocator(a->lane_arena[i]);
  }
  cas_build_sets(a);
  cas_build_jobs(a);
  a->system = audio_system_init(&a->alloc, &(AudioSystemDesc){.voice_pool = 256, .max_active = 64, .source_pool = a->clip_count + 16u});
  a->sr = (u32)a->system.output_sample_rate; // the reverb IRs and the bed are built at the device rate; the clips are 48 kHz and the mixer resamples them
  cas_mixer_setup(a);
  cas_bed_init(&a->bed, (u32)a->system.output_sample_rate, 0xBED5u);
  a->space.w[CAS_SPACE_STREET] = 0.35f;
  a->space.w[CAS_SPACE_OPEN] = 0.65f;
  a->t_volley = cod_rng_range(&a->rng, 3.0f, 11.0f);
  a->t_boom = cod_rng_range(&a->rng, 14.0f, 44.0f);
  a->t_oneshot = cod_rng_range(&a->rng, 5.0f, 17.0f);
  a->t_chatter = cod_rng_range(&a->rng, 18.0f, 50.0f);
  a->render_t0 = os_time_now();
  ecs_resource_set(w, CodAudioState, ((CodAudioState){.impl = a}));
  log_info("cod_audio: % sets, % clips, % render jobs, mixer % Hz, % lanes", fmt_u32(a->set_count), fmt_u32(a->clip_count), fmt_u32(a->job_count),
           fmt_i32(a->system.output_sample_rate), fmt_u32(a->lanes));
}

/* ------------------------------------------------------------ dev */

hz_internal void cas_meter_one(const char *name, f64 *sq, f32 *peak, u32 *frames) {
  const f32 rms = *frames ? m_sqrtf((f32)(*sq / (f64)(*frames * 2u))) : 0.0f;
  log_info("cod_audio_meter % rms_db % peak_db %", fmt_cstr(name), fmt_f32_p(cas_db(rms), 1), fmt_f32_p(cas_db(*peak), 1));
  *sq = 0.0;
  *peak = 0.0f;
  *frames = 0;
}

hz_internal void cas_meter_log(CodAudio *a, f32 t) {
  log_info("cod_audio_meter t % plays % pending % tracks % dropped %", fmt_f32_p(t, 2), fmt_u32(a->plays), fmt_u32(a->pending.len), fmt_u32(a->tracks.len),
           fmt_u32(a->dropped));
  CasBusFx *buses[5] = {&a->bus[CAS_BUS_WEAPONS], &a->bus[CAS_BUS_FOLEY], &a->bus[CAS_BUS_VOICE], &a->bus[CAS_BUS_UI], &a->send_fx.tail};
  const char *names[5] = {"sfx", "foley", "voice", "ui", "reverb"};
  for (u32 i = 0; i < 5; i++)
    cas_meter_one(names[i], &buses[i]->meter_sq, &buses[i]->meter_peak, &buses[i]->meter_frames);
  cas_meter_one("master", &a->master.meter_sq, &a->master.meter_peak, &a->master.meter_frames);
}

/* ------------------------------------------------------------ the frame */

void cod_audio_frame(EcsWorld *w) {
  CodAudioState *st = ecs_resource_get_mut(w, CodAudioState);
  CodAudio *a = st->impl;
  if (!a)
    return;
  if (!a->sources_made) {
    if (a->job_done >= a->job_count)
      cas_make_sources(a, st);
    return;
  }
  const f32 dt = m_minf(ecs_resource_get(w, AppMemory)->dt, 0.1f);
  if (!a->active) {
    a->cmds.len = 0;
    return;
  }
  if (!handle_is_valid(a->bed_voice))
    cas_bed_start(a);

  a->clock += dt;
  cas_mixer_update(a, dt);

  // commands become pending voices; delayed ones wait out their flight
  for (u32 i = 0; i < a->cmds.len; i++) {
    const CasCmd *c = &a->cmds.items[i];
    if (c->delay > 0.002f && a->pending.len < CAS_MAX_PENDING)
      a->pending.items[a->pending.len++] = (CasPending){.cmd = *c, .left = c->delay};
    else
      cas_start_cmd(a, c);
  }
  a->cmds.len = 0;
  for (u32 i = 0; i < a->pending.len;) {
    CasPending *p = &a->pending.items[i];
    p->left -= dt;
    if (p->left <= 0.0f) {
      p->cmd.pan = p->cmd.positional ? cas_pan(a, p->cmd.pos) : p->cmd.pan;
      cas_start_cmd(a, &p->cmd);
      *p = a->pending.items[--a->pending.len];
      continue;
    }
    i++;
  }

  cas_tracks_update(a);
  audio_voice_set_volume(&a->system, a->bed_voice, CAS_BED_VOLUME * a->amb_duck);
  cas_bed_feed(a, dt);
  audio_system_update(&a->system, dt);

  if (a->meter) {
    a->meter_timer -= dt;
    if (a->meter_timer <= 0.0f) {
      a->meter_timer = 0.5f;
      cas_meter_log(a, (f32)a->clock);
    }
  }
}
