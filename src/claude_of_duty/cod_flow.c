// Claude of Duty flow: frame clock, staged load, pause and pointer-lock policy, settings, dev switches.

// the presets: render scale becomes a TAAU tier (2/3 on High, Ultra); Ultra keeps a 1.5 max dpr
hz_internal CodQualityPreset cod_quality_preset(u32 q) {
  switch (q) {
  case COD_QUALITY_LOW: return (CodQualityPreset){RENDERER_UPSCALE_PERFORMANCE, 1024, 3, 60, false, false, false, false, false, true, 4, 2000, 64, 1.0f};
  case COD_QUALITY_MEDIUM: return (CodQualityPreset){RENDERER_UPSCALE_BALANCED, 2048, 3, 90, true, true, false, true, true, true, 8, 6000, 128, 1.0f};
  case COD_QUALITY_HIGH: return (CodQualityPreset){RENDERER_UPSCALE_QUALITY, 2048, 4, 140, true, true, true, true, true, true, 16, 12000, 256, 1.25f};
  default: return (CodQualityPreset){RENDERER_UPSCALE_QUALITY, 4096, 4, 200, true, true, true, true, true, true, 16, 24000, 512, 1.5f};
  }
}

// the yaw/pitch that look from `eye` onto `target`
hz_internal void cod_aim_at(v3 eye, v3 target, f32 *yaw, f32 *pitch) {
  v3 rel = v3_sub(target, eye);
  *yaw = m_atan2f(-rel.x, -rel.z);
  *pitch = m_atan2f(rel.y, m_sqrtf(rel.x * rel.x + rel.z * rel.z));
}

// the preset a fresh match starts on: High is the highest an iPhone 13 holds at 60 fps; desktops run Ultra
hz_internal i32 cod_quality_default(void) { return os_is_mobile() ? COD_QUALITY_HIGH : COD_QUALITY_ULTRA; }

hz_internal CodSettings cod_settings_sanitise(CodSettings s) {
  if (s.quality < 0 || s.quality >= COD_QUALITY_COUNT)
    s.quality = cod_quality_default();
  s.sensitivity = m_clampf(s.sensitivity, 0.2f, 3.0f);
  s.fov = m_clampf(s.fov, 65.0f, 120.0f); // Field Of View slider
  s.volume = m_clampf(s.volume, 0.0f, 1.0f);
  return s;
}

// a pointer lock/unlock request: the main lane applies each request once, after the scope
hz_internal void cod_flow_lock(CodFlow *f, b32 lock) {
  f->lock_value = lock;
  f->lock_serial++;
}

hz_internal void cod_flow_pause(CodFlow *f) {
  f->state = COD_STATE_PAUSED;
  f->audio_suspended = true;
  cod_flow_lock(f, false);
  log_info("match: paused");
}

hz_internal void cod_flow_resume(CodFlow *f) {
  f->state = COD_STATE_PLAYING;
  f->audio_suspended = false;
  if (!f->debug_controls)
    cod_flow_lock(f, true);
  log_info("match: resumed");
}

// every load stage reports done: the match starts
hz_internal b32 cod_load_done(const CodLoad *l) {
  for (u32 i = 0; i < COD_LOAD_STAGE_COUNT; i++)
    if (l->progress[i] < 1.0f)
      return false;
  return true;
}

hz_internal const char *cod_load_stage_name(u32 stage) {
  switch (stage) {
  case COD_LOAD_MATERIALS: return "materials";
  case COD_LOAD_WORLD: return "world";
  case COD_LOAD_WEAPONS: return "weapons";
  case COD_LOAD_SOLDIERS: return "soldiers";
  case COD_LOAD_FX: return "fx";
  case COD_LOAD_AUDIO: return "audio";
  case COD_LOAD_PIPELINES: return "pipelines";
  default: return "?";
  }
}

// the frame's dt, clamped, and the fixed steps every lane takes this frame
ecs_system(cod_clock_system, EcsResInOut(CodClock), EcsResIn(CodFlow), EcsResIn(CodInput), EcsResIn(TestClockFrame),
           EcsIn(CodDevSwitches)) {
  CodClock *c = ecs_sys_get_resource_mut(CodClock);
  const CodFlow *f = ecs_sys_get_resource(CodFlow);
  const CodInput *in = ecs_sys_get_resource(CodInput);
  const TestClockFrame *tc = ecs_sys_get_resource(TestClockFrame);
  // a capture's clock freezes after its steps; the freeze switch and an open editor hold the match
  b32 frozen = in->held || tc->frozen || (it->count > 0 && ecs_sys_get_component(CodDevSwitches, 0)->freeze);
  c->raw_dt = tc->dt;
  c->controls = f->state == COD_STATE_PLAYING && (in->captured || f->debug_controls);
  c->live = f->state == COD_STATE_PLAYING && !frozen;
  c->dt = frozen ? 0.0f : c->raw_dt;
  c->time += c->dt;
  c->frame++;
  // the match runs unlocked too; pause stops it
  c->steps = c->live ? fixed_timestep_begin(&c->fixed, c->dt) : 0;
  c->alpha = fixed_timestep_alpha(&c->fixed);
}

// the flow: the load gate, dev switches, menu requests, the pause policy
ecs_system(cod_flow_system, EcsIn(CodDevSwitches), EcsResIn(CodInput), EcsResInOut(CodFlow), EcsResInOut(CodLoad),
           EcsResInOut(CodSettings), EcsResInOut(CodMenuRequests), EcsResInOut(CodDev)) {
  const CodInput *in = ecs_sys_get_resource(CodInput);
  CodFlow *f = ecs_sys_get_resource_mut(CodFlow);
  CodLoad *load = ecs_sys_get_resource_mut(CodLoad);
  CodSettings *settings = ecs_sys_get_resource_mut(CodSettings);
  CodMenuRequests *menu = ecs_sys_get_resource_mut(CodMenuRequests);
  CodDev *d = ecs_sys_get_resource_mut(CodDev);
  if (it->count <= 0)
    return;
  const CodDevSwitches *sw = ecs_sys_get_component(CodDevSwitches, 0);

  if (sw->quality >= 0 && sw->quality != f->sw_quality) {
    CodSettings s = *settings;
    s.quality = sw->quality;
    s = cod_settings_sanitise(s);
    s.serial = settings->serial + 1;
    *settings = s;
    f->sw_quality = sw->quality;
  }

  if (f->state == COD_STATE_LOADING) {
    if (load->failed)
      return;
    f64 now = os_ticks_to_ms(os_time_now()) * 0.001;
    for (u32 i = 0; i < COD_LOAD_STAGE_COUNT; i++)
      if (load->stage_ms[i] == 0.0 && load->progress[i] >= 1.0f)
        load->stage_ms[i] = (now - load->started_s) * 1000.0;
    if (!cod_load_done(load))
      return;
    f->state = COD_STATE_PLAYING;
    d->restart_serial++;
    for (u32 i = 0; i < COD_LOAD_STAGE_COUNT; i++)
      log_info("load: % done at % ms", fmt_cstr(cod_load_stage_name(i)), fmt_f64_p(load->stage_ms[i], 1));
    log_info("load: match starts after % ms", fmt_f64_p((now - load->started_s) * 1000.0, 1));
  }

  b32 start = sw->start;
  if (start && !f->sw_start) {
    f->debug_controls = true;
    if (f->state == COD_STATE_PAUSED)
      cod_flow_resume(f);
  }
  f->sw_start = start;
  if (sw->screen != f->sw_screen) {
    if (sw->screen == 1 && f->state == COD_STATE_PLAYING)
      cod_flow_pause(f);
    else if (sw->screen == 0 && f->state == COD_STATE_PAUSED)
      cod_flow_resume(f);
    f->sw_screen = sw->screen;
  }

  if (menu->queue & COD_MENU_SETTINGS) {
    CodSettings s = cod_settings_sanitise(menu->settings);
    s.serial = settings->serial + 1;
    *settings = s;
    log_info("settings: quality %, sensitivity %, fov %, volume %, invert %", fmt_i32(s.quality), fmt_f32_p(s.sensitivity, 2),
             fmt_f32_p(s.fov, 1), fmt_f32_p(s.volume, 2), fmt_b32(s.invert_y));
  }
  if ((menu->queue & COD_MENU_RESUME) && f->state == COD_STATE_PAUSED)
    cod_flow_resume(f);
  menu->queue = 0;

  // the pause action toggles the menu; Escape also releases the lock, and losing it mid-match pauses
  b32 lost = !f->debug_controls && (in->lock_lost || in->focus_lost);
  if (f->state == COD_STATE_PLAYING && (lost || in->pause_pressed))
    cod_flow_pause(f);
  else if (f->state == COD_STATE_PAUSED && in->pause_pressed && !in->lock_lost)
    cod_flow_resume(f);
  if (in->focus_lost && f->state != COD_STATE_PLAYING)
    f->audio_suspended = true;
  else if (in->focused && !f->was_focused && f->state == COD_STATE_PLAYING)
    f->audio_suspended = false;
  f->was_focused = in->focused;
}

// the dev switches resolved once per frame: pose and look become a placement request, the rest pass through
ecs_system(cod_dev_system, EcsIn(CodDevSwitches), EcsResIn(CodFlow), EcsResIn(CodAiState), EcsResInOut(CodDev)) {
  const CodFlow *f = ecs_sys_get_resource(CodFlow);
  CodDev *d = ecs_sys_get_resource_mut(CodDev);
  if (it->count <= 0)
    return;
  const CodDevSwitches *sw = ecs_sys_get_component(CodDevSwitches, 0);
  d->state = f->state;
  d->debug_controls = f->debug_controls;
  d->god = sw->god;
  d->no_ai = sw->no_ai;
  d->weapon = sw->weapon;
  d->hud_off = sw->hud_off;
  d->fov = sw->fov;
  d->time_of_day = sw->time_of_day;
  d->weapon_trace = sw->weapon_trace;

  b32 pose = sw->pose;
  v3 feet = {sw->pose_x, sw->pose_y, sw->pose_z};
  f32 yaw = sw->pose_yaw, pitch = sw->pose_pitch;
  v3 eye = v3_add(feet, (v3){0.0f, COD_PLAYER_HEIGHT - COD_EYE_OFFSET, 0.0f});
  if (sw->look)
    cod_aim_at(eye, (v3){sw->look_x, sw->look_y, sw->look_z}, &yaw, &pitch);
  const CodAiState *ai = ecs_sys_get_resource(CodAiState);
  b32 tracking = sw->aim_soldier && ai->has_nearest;
  if (tracking)
    cod_aim_at(eye, ai->nearest_chest, &yaw, &pitch);

  if (pose != d->pose || !v3_eqv(feet, d->feet) || yaw != d->yaw || pitch != d->pitch) {
    d->pose_serial++;
    if (!tracking)
      log_info("dev: pose % feet % yaw % pitch %", fmt_b32(pose), fmt_struct(v3, &feet), fmt_f32_p(yaw, 3), fmt_f32_p(pitch, 3));
  }
  d->pose = pose;
  d->feet = feet;
  d->yaw = yaw;
  d->pitch = pitch;
}

void cod_flow_init(EcsWorld *w) {
  ecs_resource_set(w, CodFlow, ((CodFlow){.state = COD_STATE_LOADING, .was_focused = true, .sw_quality = -1}));
  CodClock clock = {.fixed = fixed_timestep_make(COD_FIXED_HZ, COD_MAX_SUBSTEPS)};
  ecs_resource_set(w, CodClock, clock);
  ecs_resource_set(w, CodLoad, ((CodLoad){.started_s = os_ticks_to_ms(os_time_now()) * 0.001}));
  ecs_resource_set(w, CodDev, ((CodDev){.weapon = -1, .time_of_day = -1.0f}));
  ecs_resource_set(w, CodSettings,
                   ((CodSettings){.quality = cod_quality_default(), .sensitivity = 1.0f, .fov = COD_FOV, .volume = 1.0f, .serial = 1}));
  ecs_resource_set(w, CodMenuRequests, ((CodMenuRequests){0}));
  ecs_resource_set(w, CodEditor, ((CodEditor){0}));
  test_clock_init(w);
  // the dev switches live on one entity so `set:CodDevSwitches.<field>` reaches them
  EcsEntity sw = ecs_entity_create(w);
  ecs_component_set(w, sw, EntityName, {.name = fixedstr64_from_cstr("DevSwitches")});
  ecs_component_set(w, sw, CodDevSwitches, {.quality = -1, .time_of_day = -1.0f, .weapon = -1});
}

void cod_flow_schedule(EcsWorld *w, f32 dt) {
  // the capture clock waits for the load: loading frames never count
  test_clock_schedule(w, dt, ecs_resource_get(w, CodFlow)->state != COD_STATE_LOADING);
  ecs_schedule_single(w, cod_flow_system);  // writes the CodFlow, CodLoad, CodSettings, CodDev resources
  ecs_schedule_single(w, cod_clock_system); // writes the CodClock resource
  ecs_schedule_single(w, cod_dev_system);   // writes the CodDev resource
}

// main lane after the scope: the flow's pointer-lock requests (a window-system call)
void cod_flow_main(EcsWorld *w, u32 *lock_seen) {
  const CodFlow *f = ecs_resource_get(w, CodFlow);
  if (f->lock_serial != *lock_seen) {
    *lock_seen = f->lock_serial;
    // touch has no pointer to lock; a later mouse click locks through the input policy
    if (!ecs_resource_get(w, CodInput)->touch)
      os_lock_mouse(f->lock_value);
  }
}
