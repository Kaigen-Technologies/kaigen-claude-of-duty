// Claude of Duty HUD and menus: init, the schedule hooks and the UI frame.

hz_internal GpuPipeline cod_hud_pipeline(UISystem *ui, GpuPipelineDesc d, b32 screen_blend) {
  mem_set(d.blend_attachments, 0, sizeof(d.blend_attachments));
  d.vs_uses_bindless = false;
  d.cull_mode = GPU_CULL_NONE;
  d.depth_test = false;
  d.depth_write = false;
  d.blend = (GpuBlendState){
      .enabled = true,
      .src_factor = screen_blend ? GPU_BLEND_ONE : GPU_BLEND_SRC_ALPHA,
      .dst_factor = screen_blend ? GPU_BLEND_ONE_MINUS_SRC_COLOR : GPU_BLEND_ONE_MINUS_SRC_ALPHA,
      .op = GPU_BLEND_OP_ADD,
      .src_factor_alpha = screen_blend ? GPU_BLEND_ZERO : GPU_BLEND_ONE,
      .dst_factor_alpha = screen_blend ? GPU_BLEND_ONE : GPU_BLEND_ONE_MINUS_SRC_ALPHA,
      .op_alpha = GPU_BLEND_OP_ADD,
  };
  GpuRenderTarget rt = ui_get_render_target(ui);
  if (handle_is_valid(rt)) {
    d.color_attachment_count = 1;
    d.color_formats[0] = gpu_get_render_target_format(rt, 0);
  }
  return gpu_make_pipeline(&d);
}

hz_internal HzHandleT(UIFontFamily) cod_hud_load_family(UISystem *ui, const char *const *names, u32 count) {
  return ui_load_family_os_list(ui, names, count, NULL); // names are copied
}

void cod_hud_init(EcsWorld *w, Allocator *alloc) {
  CodUi *u = ALLOC(alloc, CodUi);
  u->ui = ui_create(&(UIInitDesc){.max_glyphs = 8192, .backdrop_filters = true}, ecs_resource_get_mut(w, AssetSystem));
  ui_system_attach_to_renderer(u->ui, ecs_resource_get_mut(w, RendererState));

  // the three stacks; the first installed family wins
  TempAllocator ta = tctx_temp_allocator_begin(NULL);
  const char **body = ALLOC_ARRAY(&ta.allocator, const char *, 6);
  body[0] = "Avenir Next Condensed";
  body[1] = "DIN Alternate";
  body[2] = "Roboto Condensed";
  body[3] = "Arial Narrow";
  body[4] = "Helvetica Neue";
  body[5] = "Inter";
  u->body = cod_hud_load_family(u->ui, body, 6);
  const char **display = ALLOC_ARRAY(&ta.allocator, const char *, 6);
  display[0] = "DIN Condensed";
  display[1] = "Avenir Next Condensed";
  display[2] = "Oswald";
  display[3] = "Arial Narrow";
  display[4] = "Helvetica Neue";
  display[5] = "Impact";
  u->display = cod_hud_load_family(u->ui, display, 6);
  const char **mono = ALLOC_ARRAY(&ta.allocator, const char *, 3);
  mono[0] = "SF Mono";
  mono[1] = "Roboto Mono";
  mono[2] = "Menlo";
  u->mono = cod_hud_load_family(u->ui, mono, 3);
  tctx_temp_allocator_end(ta);

  u->band_pipeline = cod_hud_pipeline(u->ui, *shader_cod_ui_band_pipeline_desc(), false);
  u->hurt_pipeline = cod_hud_pipeline(u->ui, *shader_cod_ui_hurt_pipeline_desc(), false);
  u->hurt_screen_pipeline = cod_hud_pipeline(u->ui, *shader_cod_ui_hurt_pipeline_desc(), true);
  u->map_pipeline = cod_hud_pipeline(u->ui, *shader_cod_ui_minimap_pipeline_desc(), false);
  u->fx.hud_visible = 1.0f;
  u->fx.visible = 1.0f;
  u->fx.flash_t = 1.0f;
  u->fx.regen_t = 1.0f;
  u->cursor = OS_CURSOR_ARROW;

  HudMsgDesc msgs = {0};
  msgs.channels[HUD_MSG_BANNER] = (HudMsgChannelDesc){.policy = HUD_MSG_POLICY_REPLACE, .enter_s = 0.16f, .exit_s = 0.462f, .restart_on_replace = true};
  msgs.channels[HUD_MSG_HINT] = (HudMsgChannelDesc){.policy = HUD_MSG_POLICY_REPLACE, .enter_s = 0.25f, .exit_s = 0.25f};
  hud_msg_init(w, &msgs);
  world_ui_init(w, &(WorldUiDesc){0});
  ecs_resource_set_zero(w, CodHudModel);
  ecs_resource_set_ref(w, CodUi, u);
  ecs_resource_set_ref(w, UISystem, u->ui);
  cod_touch_init(w, u->ui);
}

void cod_hud_font_update(EcsWorld *w) { ui_font_update(ecs_resource_get(w, CodUi)->ui); }

void cod_hud_schedule(EcsWorld *w) {
  ecs_schedule_single(w, cod_hud_switches_system); // one writer of the HUD model's dev flags
  ecs_schedule_single(w, cod_hud_model_system);    // consolidation: the model is one resource every widget reads
  ecs_schedule_single(w, cod_hud_world_system);    // the score and the explosion flinch fold into the same model
  ecs_schedule_single(w, cod_hud_soldiers_system); // per-soldier events fold into the model's shared event arrays
}

void cod_hud_schedule_late(EcsWorld *w) {
  world_ui_schedule(w);
  hud_msg_schedule(w);
  ecs_schedule_single(w, cod_hud_pins_system);    // gathers the projected pins into the model's array
  ecs_schedule_single(w, cod_hud_nades_system);   // gathers the projected grenades into the model's array
  ecs_schedule_single(w, cod_hud_numbers_system); // gathers the projected damage numbers into the model's array
  ecs_schedule_single(w, cod_hud_prompt_system);  // picks the one prompt into the model
}

// ---- the retained widgets: events in, one step of every widget ----

hz_internal void cod_hud_fx_events(CodHudFx *fx, const CodHudModel *m) {
  for (u32 i = 0; i < m->hit_count; i++)
    cod_hud_hit_spawn(fx, m->hits[i]);
  for (u32 i = 0; i < m->hurt_count; i++) {
    const CodHudHurt *h = &m->hurts[i];
    if (h->damage) {
      f32 s = m_saturatef(h->amount / 40.0f);
      cod_hud_arc_spawn(fx, h->dx, h->dz, 0.45f + s * 0.55f);
      cod_hud_health_on_damage(fx, s);
      fx->kick_vel += 30.0f * (0.5f + s);
    }
  }
  for (u32 i = 0; i < m->kill_count; i++)
    cod_hud_kill_push(fx, &m->kills[i]);
  for (u32 i = 0; i < m->fire_count; i++) {
    fx->kick_vel += 78.0f;
    fx->kick = m_minf(fx->kick + 1.2f, 16.0f);
  }
  if (m->flinch > 0.0f)
    fx->kick_vel += 30.0f * m->flinch;
}

hz_internal void cod_hud_fx_step(CodHudFx *fx, const CodHudModel *m, const CodHudView *v) {
  if (!fx->started) {
    fx->started = true;
    f32 max_h = m->max_health > 0.0f ? m->max_health : 100.0f;
    fx->hp_shown = m_saturatef(m->health / max_h);
    fx->armour_shown = m->armour > 0.0f ? 1.0f : 0.0f;
    fx->last_ammo = (i32)m->ammo;
    fx->prev_regen = m->regen;
  }
  if (m->regen && !fx->prev_regen)
    fx->regen_t = 0.0f;
  fx->prev_regen = m->regen;
  cod_hud_fx_events(fx, m);
  f32 dt = v->dt;
  cod_hud_crosshair_update(fx, m, dt, v->k);
  cod_hud_hits_update(fx, dt);
  cod_hud_arcs_update(fx, dt);
  cod_hud_health_update(fx, m, dt);
  cod_hud_ammo_update(fx, m, dt);
  cod_hud_kills_update(fx, dt);
  cod_hud_prompt_update(fx, m, dt);
}

// ---- the UI frame ----

hz_internal void cod_hud_draw_loading(const CodUi *u, const CodWorld *world, const CodHudView *v) {
  f32 k = v->k;
  ui_element({.layout = {.sizing = cod_fixed2(v->w, v->h),
                         .layout_direction = UI_TOP_TO_BOTTOM,
                         .child_alignment = {.x = UI_ALIGN_X_CENTER, .y = UI_ALIGN_Y_CENTER}},
              .floating = cod_float_root(UI_ATTACH_POINT_LEFT_TOP, (v2){0, 0}, COD_Z_LOADING),
              .background_color = cod_rgba(4, 6, 8, 0.96f)}) {
    HzUITextConfig t = cod_text(u, COD_FONT_DISPLAY, 30.0f * k, UI_FONT_BOLD, COD_INK, 0.3f * 30.0f * k);
    ui_text(ui_string("LOADING"), cod_o2(t, k));
    ui_element({.layout = {.sizing = cod_fixed2(120.0f * k, 1.5f * k), .layout_direction = UI_STACK, .margin = {.top = (u16)(12.0f * k + 0.5f)}},
                .background_color = cod_rgba(255, 255, 255, 0.16f)}) {
      ui_element({.layout = {.sizing = {ui_sizing_grow(0), ui_sizing_grow(0)}},
                  .background_color = COD_AMBER,
                  .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {m_saturatef(world->progress), 1.0f}, .origin = {0.0001f, 0.5f}}))}) {}
    }
  }
}

hz_internal void cod_hud_draw_click_hint(const CodUi *u, const HudMessages *msgs, const CodHudView *v) {
  HudMsgView mv;
  if (!hud_msg_view(msgs, HUD_MSG_HINT, &mv))
    return;
  f32 k = v->k;
  ui_element({.floating = cod_float_root(UI_ATTACH_POINT_CENTER_CENTER, (v2){0, v->h * 0.08f}, COD_Z_CHROME),
              .opacity = opt_some(f32, ease_quad_out(mv.level)),
              .pointer_capture_mode = COD_PASS}) {
    ui_text(cod_ui_str(mv.msg->title.value), cod_o2(cod_text(u, COD_FONT_DISPLAY, 22.0f * k, UI_FONT_BOLD, COD_INK, 0.3f * 22.0f * k), k));
  }
}

hz_internal void cod_hud_click_hint_update(EcsWorld *w, CodUi *u, b32 want) {
  if (want == u->click_hint)
    return;
  u->click_hint = want;
  if (want) {
    HudMsg msg = {.channel = HUD_MSG_HINT, .duration = 0.0f};
    msg.title = fixedstr256_from_cstr((CString) "CLICK TO PLAY");
    hud_msg_push(w, &msg);
  } else {
    hud_msg_clear(w, HUD_MSG_HINT);
  }
}

void cod_hud_frame(EcsWorld *w) {
  CodUi *u = ecs_resource_get_mut(w, CodUi);
  const CodFlow *flow = ecs_resource_get(w, CodFlow);
  const CodInput *in = ecs_resource_get(w, CodInput);
  const CodHudModel *m = ecs_resource_get(w, CodHudModel);
  const CodWorld *world = ecs_resource_get(w, CodWorld);
  const CodSettings *settings = ecs_resource_get(w, CodSettings);
  const HudMessages *msgs = ecs_resource_get(w, HudMessages);
  CodMenuRequests *req = ecs_resource_get_mut(w, CodMenuRequests);
  const AppMemory *mem = ecs_resource_get(w, AppMemory);
  InputSystem *input = ecs_resource_get_mut(w, InputSystem);
  ui_begin_frame(u->ui, input, mem->dt);

  CodHudView v = {.w = (f32)u->ui->canvas_width, .h = (f32)u->ui->canvas_height, .dt = m->dt, .time = m->time};
  v.k = m_clampf(v.h / 1080.0f, 0.62f, 2.4f);
  v.touch = in->touch;
  v.safe = os_get_screen_desc().safe_area;
  v.raw_dt = m_minf(m->raw_dt, 0.1f);
  CodHudFx *fx = &u->fx;
  CodHudMenu *menu = &u->menu;

  b32 playing = flow->state == COD_STATE_PLAYING;
  b32 menu_open = flow->state == COD_STATE_PAUSED;
  cod_hud_fx_step(fx, m, &v);
  menu->shown = m_damp(menu->shown, menu_open ? 1.0f : 0.0f, 14.0f, v.raw_dt);
  f32 goal = (m->hud_off ? 0.0f : 1.0f) * (menu_open ? 0.15f : 1.0f);
  fx->hud_visible = m_damp(fx->hud_visible, goal, 10.0f, v.raw_dt);
  v.vis = fx->hud_visible;

  b32 dev_run = ecs_resource_get(w, CodDev)->debug_controls;
  cod_hud_click_hint_update(w, u, playing && !in->captured && !dev_run);
  u->cursor = OS_CURSOR_ARROW;

  if (flow->state == COD_STATE_LOADING) {
    cod_hud_draw_loading(u, world, &v);
  } else {
    if (!u->load_reported) {
      u->load_reported = true;
      f64 now = os_ticks_to_ms(os_time_now()) * 0.001;
      log_info("load: screen closes after % ms", fmt_f64_p((now - ecs_resource_get(w, CodLoad)->started_s) * 1000.0, 1));
    }
    cod_hud_draw_hurt(u, fx, &v);
    ui_element({.opacity = opt_some(f32, v.vis), .pointer_capture_mode = COD_PASS}) {
      f32 k = v.k;
      for (u32 i = 0; i < COD_HUD_PINS; i++)
        if (m->pins[i].active)
          cod_hud_draw_pin(u, &m->pins[i], k);
      for (u32 i = 0; i < COD_HUD_NADES; i++)
        if (m->nades[i].active)
          cod_hud_draw_nade(u, &m->nades[i], k);
      for (u32 i = 0; i < COD_HUD_NUMBERS; i++)
        if (m->numbers[i].active)
          cod_hud_draw_number(u, &m->numbers[i], k);
      cod_hud_draw_crosshair(fx, k);
      for (u32 i = 0; i < COD_HUD_HITS; i++)
        if (fx->hits[i].alive)
          cod_hud_draw_hitmarker(&fx->hits[i], k);
      for (u32 i = 0; i < COD_HUD_ARCS; i++)
        if (fx->arcs[i].alive)
          cod_hud_draw_arc(&fx->arcs[i], m, k);
      cod_hud_draw_prompt(u, fx, k, v.h);
      cod_hud_draw_compass(u, m, &v);
      cod_hud_draw_match(u, m, &v);
      cod_hud_draw_minimap(u, m, world, &v);
      cod_hud_draw_vitals(u, fx, m, &v);
      cod_hud_draw_ammo(u, fx, m, &v);
      cod_hud_draw_killfeed(u, fx, &v);
      cod_hud_draw_banner(u, msgs, &v);
      cod_hud_draw_click_hint(u, msgs, &v);
    }
    if (playing && v.touch && !m->hud_off)
      cod_hud_draw_touch(u, ecs_resource_get(w, CodTouch), m);
    if (menu->shown >= 0.004f) {
      i32 level = (i32)MIN((u32)renderer_quality(ecs_resource_get(w, RendererState)), (u32)COD_QUALITY_ULTRA);
      CodMenuFrame mf = {.u = u, .input = input, .cur = settings, .level = level, .req = req, .k = v.k, .raw_dt = v.raw_dt,
                         .touch = v.touch};
      cod_hud_draw_menu(&mf, &v, ease_quad_out(menu->shown));
      if (mf.hand)
        u->cursor = OS_CURSOR_HAND;
    }
  }
  cod_hud_draw_fps(u, mem->dt, &v);
  if (m->stats)
    perf_stats_draw(input, mem->dt);
  ui_finish_layout(u->ui);
  if (!in->captured)
    os_set_cursor(u->cursor);
}
