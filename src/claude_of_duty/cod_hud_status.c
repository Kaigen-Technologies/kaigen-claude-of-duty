// Claude of Duty HUD status widgets: the hurt layers and vitals, the ammo panel, the killfeed.

// a piecewise-linear profile of up to five (position, alpha) stops, packed the way the band shader reads them
typedef struct CodStops {
  u32 n;
  vec4 t0, t1, a0, a1;
} CodStops;

hz_internal CodStops cod_stops(u32 n, f32 t0, f32 a0, f32 t1, f32 a1, f32 t2, f32 a2, f32 t3, f32 a3, f32 t4, f32 a4) {
  return (CodStops){.n = n, .t0 = {t0, t1, t2, t3}, .t1 = {t4, 0, 0, 0}, .a0 = {a0, a1, a2, a3}, .a1 = {a4, 0, 0, 0}};
}

hz_internal void cod_pack_stops(const CodStops *s, vec4 *t0, vec4 *t1, vec4 *a0, vec4 *a1) {
  *t0 = s->t0;
  *t1 = s->t1;
  *a0 = s->a0;
  *a1 = s->a1;
}

// the masked scrim: `color` (bytes, a = the peak alpha 0..1) times a vertical profile, a horizontal one and a css mask
hz_internal void cod_band_fill(const CodUi *u, ShaderCodUiBandBandParams *p, HzUIColor color, f32 alpha, const CodStops *v, const CodStops *h,
                               const CodStops *m) {
  p->color = (vec4){color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, alpha};
  cod_pack_stops(v, &p->vt0, &p->vt1, &p->va0, &p->va1);
  cod_pack_stops(h, &p->ht0, &p->ht1, &p->ha0, &p->ha1);
  cod_pack_stops(m, &p->mt0, &p->mt1, &p->ma0, &p->ma1);
  p->counts = (vec4){(f32)v->n, (f32)h->n, (f32)m->n, 0};
  (void)u;
}

hz_internal hz_noinline void cod_band_element(const CodUi *u, HzUISizing sizing, HzUIFloating floating, HzUIColor color, f32 alpha,
                                              const CodStops *v, const CodStops *h, const CodStops *m) {
  ShaderCodUiBandBandParams p = {0};
  cod_band_fill(u, &p, color, alpha, v, h, m);
  ui_element({.layout = {.sizing = sizing},
              .floating = floating,
              .shader = {.pipeline = u->band_pipeline, .uniforms = &p, .uniforms_size = sizeof(p)},
              .pointer_capture_mode = COD_PASS}) {}
}

// ----: the hurt state ----

hz_internal void cod_hud_health_update(CodHudFx *fx, const CodHudModel *m, f32 dt) {
  f32 max_h = m->max_health > 0.0f ? m->max_health : 100.0f;
  f32 h = m_saturatef(m->health / max_h);
  f32 target_hurt = m_powf(m_saturatef((0.78f - h) / 0.78f), 1.3f);
  fx->hurt = m_damp(fx->hurt, target_hurt, 7.0f, dt);

  // the heartbeat: systole and a weaker diastole
  f32 beat_i = m_saturatef((0.5f - h) / 0.5f);
  if (beat_i > 0.02f) {
    fx->beat_phase += dt * m_lerpf(1.15f, 2.35f, beat_i);
    f32 p = fx->beat_phase - m_floorf(fx->beat_phase);
    f32 a = p / 0.085f, b = (p - 0.235f) / 0.1f;
    fx->beat_energy = (m_expf(-a * a) + 0.55f * m_expf(-b * b)) * beat_i;
  } else {
    fx->beat_energy = m_damp(fx->beat_energy, 0.0f, 6.0f, dt);
    fx->beat_phase = 0.0f;
  }

  if (fx->regen_t < 1.0f)
    fx->regen_t = m_minf(1.0f, fx->regen_t + dt / 1.8f);
  fx->regen_pulse = m->regen ? 0.12f * (1.0f - ease_cubic_out(fx->regen_t)) : 0.0f;

  if (fx->flash_t < 1.0f)
    fx->flash_t = m_minf(1.0f, fx->flash_t + dt / 0.19f);
  fx->hp_shown = m_damp(fx->hp_shown, h, 16.0f, dt);
  fx->armour_shown = m_damp(fx->armour_shown, m->armour > 0.0f ? 1.0f : 0.0f, 10.0f, dt);
}

// a damage hit: the screen flash and the reticle flinch (the arc is spawned by the caller)
hz_internal void cod_hud_health_on_damage(CodHudFx *fx, f32 intensity) {
  fx->flash_t = 0.0f;
  fx->flash_peak = 0.35f + 0.65f * m_saturatef(intensity);
}

hz_internal hz_noinline void cod_hud_draw_hurt_quad(const CodUi *u, GpuPipeline pipeline, v2 box, v2 at_centre, f32 mode, f32 opacity, f32 scale) {
  ShaderCodUiHurtHurtParams p = {0};
  p.box = (vec4){box.x, box.y, mode, opacity};
  p.warp = (vec4){34.0f, 0.006f, 0.011f, 17.0f};
  ui_element({.layout = {.sizing = cod_fixed2(box.x, box.y)},
              .floating = cod_float_centre(UI_ATTACH_POINT_CENTER_CENTER, at_centre, COD_Z_HURT),
              .shader = {.pipeline = pipeline, .uniforms = &p, .uniforms_size = sizeof(p)},
              .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {scale, scale}})),
              .pointer_capture_mode = COD_PASS}) {}
}

// the layer under the HUD: blood vignette, low-health beat, desaturation, hit flash (never faded with the HUD)
hz_internal hz_noinline void cod_hud_draw_hurt(const CodUi *u, const CodHudFx *fx, const CodHudView *v) {
  f32 blood_a = m_saturatef(fx->hurt * 1.05f + fx->beat_energy * 0.16f);
  if (blood_a >= 0.004f) {
    // the wrapper bleeds 7 % past the canvas on every side and pulses with the heartbeat
    f32 bs = 1.0f + fx->beat_energy * 0.022f + fx->regen_pulse * 0.12f;
    cod_hud_draw_hurt_quad(u, u->hurt_pipeline, (v2){v->w * 1.14f, v->h * 1.14f}, (v2){0, 0}, 0.0f, blood_a, bs);
  }
  f32 beat_a = m_saturatef(fx->beat_energy * 0.55f);
  if (beat_a >= 0.004f)
    cod_hud_draw_hurt_quad(u, u->hurt_pipeline, (v2){v->w, v->h}, (v2){0, 0}, 2.0f, beat_a, 1.0f);
  f32 desat_a = m_saturatef(fx->hurt * 0.8f);
  if (desat_a >= 0.01f) {
    // backdrop-filter: saturate(.6) contrast(1.04) brightness(.97); the contrast has no engine filter
    ui_element({.layout = {.sizing = cod_fixed2(v->w, v->h)},
                .floating = cod_float_layer(COD_Z_HURT),
                .background_color = cod_rgba(0, 0, 0, 0.03f),
                .backdrop_saturate = opt_some(f32, 0.6f),
                .opacity = opt_some(f32, desat_a),
                .pointer_capture_mode = COD_PASS}) {}
  }
  if (fx->flash_t < 1.0f) {
    f32 a = fx->flash_peak * (1.0f - ease_quad_out(fx->flash_t)) * 0.8f;
    cod_hud_draw_hurt_quad(u, u->hurt_screen_pipeline, (v2){v->w, v->h}, (v2){0, 0}, 1.0f, a, 1.0f);
  }
}

// ---- vitals (bottom left) ----

typedef struct CodVitalsStyle {
  HzUIColor number;
  HzUIColor fill_a, fill_b, fill_c;
  b32 three; // the white fill has a middle stop
} CodVitalsStyle;

hz_internal CodVitalsStyle cod_vitals_style(f32 h) {
  if (h <= 0.28f)
    return (CodVitalsStyle){COD_RED, {255, 139, 122, 255}, {224, 36, 20, 255}, {0}, false};
  if (h <= 0.55f)
    return (CodVitalsStyle){COD_AMBER, {255, 217, 138, 255}, {242, 160, 28, 255}, {0}, false};
  return (CodVitalsStyle){COD_INK, {251, 253, 252, 255}, {225, 231, 228, 255}, {179, 188, 185, 255}, true};
}

hz_internal hz_noinline void cod_hud_draw_vitals_head(const CodUi *u, const CodHudFx *fx, const CodHudModel *m, f32 k, f32 h) {
  CodVitalsStyle st = cod_vitals_style(h);
  f32 max_h = m->max_health > 0.0f ? m->max_health : 100.0f;
  f32 hp = m_maxf(0.0f, m_minf(max_h, m->health));
  f32 pulse = 1.0f + fx->beat_energy * 0.05f;
  ui_element({.layout = {.sizing = {.width = ui_sizing_grow(0)},
                         .child_alignment = {.y = UI_ALIGN_Y_BASELINE},
                         .margin = {.bottom = (u16)(4.4f * k + 0.5f)}},
              .pointer_capture_mode = COD_PASS}) {
    ui_text(ui_string("HEALTH"), cod_o1(cod_text(u, COD_FONT_BODY, 9.5f * k, UI_FONT_SEMIBOLD, COD_INK2, 0.24f * 9.5f * k), k));
    ui_element({.layout = {.sizing = {.width = ui_sizing_grow(0)}}, .pointer_capture_mode = COD_PASS}) {}
    ui_element({.layout = {.child_alignment = {.y = UI_ALIGN_Y_BASELINE}},
                .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {pulse, pulse}})),
                .pointer_capture_mode = COD_PASS}) {
      HzUITextConfig num = cod_text(u, COD_FONT_DISPLAY, 26.0f * k, UI_FONT_BOLD, st.number, 0.02f * 26.0f * k);
      num.lineHeight = 0.85f * 26.0f * k;
      ui_text(cod_str_u32((u32)m_roundf(hp)), cod_o2_bloom(num, k, 12.0f, 0.5f));
      ui_element({.layout = {.margin = {.left = (u16)(2.0f * k + 0.5f)}}, .pointer_capture_mode = COD_PASS}) {
        HzUITextConfig of = cod_text(u, COD_FONT_BODY, 11.0f * k, UI_FONT_SEMIBOLD, COD_INK3, 0.1f * 11.0f * k);
        of.lineHeight = 0.85f * 11.0f * k;
        String32Bytes s = {0};
        cstr_format(s.value, sizeof(s.value), "/%", fmt_u32((u32)m_roundf(max_h)));
        ui_text(cod_ui_str(s.value), of);
      }
    }
  }
}

hz_internal hz_noinline void cod_hud_draw_vitals_track(const CodHudFx *fx, f32 k, f32 h, f32 width) {
  CodVitalsStyle st = cod_vitals_style(h);
  UIGradient fill = {.kind = UI_GRADIENT_LINEAR, .angle = 180.0f};
  if (st.three) {
    fill.stop_count = 3;
    fill.stops[0] = (HzUIGradientStop){0.0f, st.fill_a};
    fill.stops[1] = (HzUIGradientStop){0.46f, st.fill_b};
    fill.stops[2] = (HzUIGradientStop){1.0f, st.fill_c};
  } else {
    fill.stop_count = 2;
    fill.stops[0] = (HzUIGradientStop){0.0f, st.fill_a};
    fill.stops[1] = (HzUIGradientStop){1.0f, st.fill_b};
  }
  UIGradient ticks = {.kind = UI_GRADIENT_LINEAR,
                      .angle = 90.0f,
                      .repeating = true,
                      .stop_count = 4,
                      .stops = {{0.0f, COD_CLEAR}, {0.2f, COD_CLEAR}, {0.2f, cod_rgba(4, 8, 11, 0.85f)}, {0.2f, cod_rgba(4, 8, 11, 0.85f)}},
                      .stop_offset_px = {0.0f, -1.0f, -1.0f, 0.0f}};
  ui_element({.layout = {.sizing = {ui_sizing_grow(0), ui_sizing_fixed(9.0f * k)}, .layout_direction = UI_STACK},
              .background_color = cod_rgba(5, 9, 12, 0.72f),
              .clip = {.horizontal = true, .vertical = true},
              .shadow = {.color = cod_rgba(0, 0, 0, 0.55f), .spread = 1.0f, .inset = true},
              .shadows = {{.color = cod_rgba(216, 232, 240, 0.16f), .spread = 1.0f},
                          {.color = cod_rgba(0, 0, 0, 0.5f), .offset_y = 1.0f * k, .blur = 4.0f * k}},
              .pointer_capture_mode = COD_PASS}) {
    ui_element({.layout = {.sizing = {ui_sizing_grow(0), ui_sizing_grow(0)}},
                .gradient = fill,
                .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {m_saturatef(fx->hp_shown), 1.0f}, .origin = {0.0001f, 0.5f}})),
                .pointer_capture_mode = COD_PASS}) {}
    // five 20 HP segments: a dark 1 px rule at every fifth of the track
    ui_element({.layout = {.sizing = {ui_sizing_grow(0), ui_sizing_grow(0)}},
                .backgrounds = ui_backgrounds({.gradient = ticks}),
                .pointer_capture_mode = COD_PASS}) {}
  }
  (void)width;
}

hz_internal hz_noinline void cod_hud_draw_vitals_armour(const CodUi *u, const CodHudFx *fx, const CodHudModel *m, f32 k) {
  if (fx->armour_shown < 0.01f)
    return;
  f32 per = (m->max_armour > 0.0f ? m->max_armour : 150.0f) / 3.0f;
  ui_element({.layout = {.sizing = {.width = ui_sizing_grow(0)},
                         .child_gap = (u16)(5.6f * k + 0.5f),
                         .child_alignment = {.y = UI_ALIGN_Y_CENTER},
                         .margin = {.top = (u16)(6.0f * k + 0.5f)}},
              .opacity = opt_some(f32, fx->armour_shown),
              .pointer_capture_mode = COD_PASS}) {
    ui_text(ui_string("ARMOUR"), cod_o1(cod_text(u, COD_FONT_BODY, 9.5f * k, UI_FONT_SEMIBOLD, cod_rgba(150, 206, 238, 0.7f), 0.24f * 9.5f * k), k));
    ui_element({.layout = {.sizing = {.width = ui_sizing_grow(0)}, .child_gap = (u16)(3.2f * k + 0.5f)}, .pointer_capture_mode = COD_PASS}) {
      for (u32 i = 0; i < 3; i++) {
        f32 f = m_saturatef((m->armour - (f32)i * per) / per);
        ui_element({.layout = {.sizing = {ui_sizing_grow(0), ui_sizing_fixed(5.0f * k)}, .layout_direction = UI_STACK},
                    .background_color = cod_rgba(5, 9, 12, 0.7f),
                    .clip = {.horizontal = true, .vertical = true},
                    .shadow = {.color = cod_rgba(0, 0, 0, 0.5f), .spread = 1.0f, .inset = true},
                    .shadows = {{.color = cod_rgba(121, 190, 230, 0.18f), .spread = 1.0f}},
                    .pointer_capture_mode = COD_PASS}) {
          ui_element({.layout = {.sizing = {ui_sizing_grow(0), ui_sizing_grow(0)}},
                      .gradient = {.kind = UI_GRADIENT_LINEAR,
                                   .angle = 180.0f,
                                   .stop_count = 2,
                                   .stops = {{0.0f, {189, 233, 255, 255}}, {1.0f, {59, 166, 226, 255}}}},
                      .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {f, 1.0f}, .origin = {0.0001f, 0.5f}})),
                      .opacity = opt_some(f32, f > 0.001f ? 1.0f : 0.0f),
                      .pointer_capture_mode = COD_PASS}) {}
        }
      }
    }
  }
}

// touch: the two status panels meet at the bottom centre, between the stick and the buttons
#define COD_TOUCH_STATUS_GAP 10.0f

hz_internal hz_noinline void cod_hud_draw_vitals(const CodUi *u, const CodHudFx *fx, const CodHudModel *m, const CodHudView *v) {
  f32 k = v->k;
  f32 max_h = m->max_health > 0.0f ? m->max_health : 100.0f;
  f32 h = m_saturatef(m->health / max_h);
  f32 bottom = -(COD_PAD * k + v->safe.bottom);
  HzUIFloating at = v->touch ? cod_float_root_at(UI_ATTACH_POINT_RIGHT_BOTTOM, UI_ATTACH_POINT_CENTER_BOTTOM, (v2){-COD_TOUCH_STATUS_GAP, bottom}, COD_Z_CHROME)
                             : cod_float_root(UI_ATTACH_POINT_LEFT_BOTTOM, (v2){COD_PAD * k + v->safe.left, bottom}, COD_Z_CHROME);
  ui_element({.layout = {.sizing = {.width = ui_sizing_fixed(196.0f * k)}, .layout_direction = UI_TOP_TO_BOTTOM},
              .floating = at,
              .pointer_capture_mode = COD_PASS}) {
    cod_hud_draw_vitals_head(u, fx, m, k, h);
    cod_hud_draw_vitals_track(fx, k, h, 196.0f * k);
    cod_hud_draw_vitals_armour(u, fx, m, k);
  }
}

// ---- ammo (bottom right) ----

hz_internal void cod_hud_ammo_update(CodHudFx *fx, const CodHudModel *m, f32 dt) {
  i32 ammo = (i32)m->ammo;
  if (fx->last_ammo != ammo) {
    if (fx->started && ammo < fx->last_ammo)
      fx->punch = 1.0f;
    fx->last_ammo = ammo;
  }
  fx->punch = m_maxf(0.0f, fx->punch - dt * 6.5f);
}

// the frag icon (viewBox 16 x 20 in a 13 x 16.5 box)
hz_internal hz_noinline void cod_hud_draw_frag(const CodHudModel *m, f32 k) {
  f32 s = 0.8125f * k;
  v2 o = {0.0f, 0.125f * k};
  HzUIColor white = cod_rgba(255, 255, 255, 0.92f);
  cod_shape_poly8(s, o, (v2){6.4f, 0}, (v2){9.6f, 0}, (v2){9.6f, 2.1f}, (v2){11.1f, 2.1f}, (v2){12.2f, 4.1f}, (v2){3.8f, 4.1f},
                  (v2){4.9f, 2.1f}, (v2){6.4f, 2.1f}, white);
  ui_element({.layout = {.sizing = cod_fixed2(11.2f * s, 15.4f * s)},
              .floating = cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){o.x + 2.4f * s, o.y + 4.6f * s}),
              .background_color = white,
              .corner_radius = ui_corner_radius(5.6f * s),
              .shadow = {.color = cod_rgba(0, 0, 0, 0.95f), .blur = 4.0f * k},
              .pointer_capture_mode = COD_PASS}) {}
  HzUIColor dark = cod_rgba(0, 0, 0, 0.5f);
  cod_shape_box(s, o, 3.0f, 9.05f, 10.0f, 0.9f, dark, 0.0f);
  cod_shape_box(s, o, 3.0f, 12.55f, 10.0f, 0.9f, dark, 0.0f);
  cod_shape_box(s, o, 3.0f, 15.75f, 10.0f, 0.9f, dark, 0.0f);
  cod_shape_box(s, o, 7.55f, 5.0f, 0.9f, 14.6f, dark, 0.0f);
  (void)m;
}

// the flashbang icon
hz_internal hz_noinline void cod_hud_draw_flash(f32 k) {
  f32 s = 0.8125f * k;
  v2 o = {0.0f, 0.125f * k};
  HzUIColor white = cod_rgba(255, 255, 255, 0.92f);
  cod_shape_box(s, o, 6.2f, 0.0f, 3.6f, 2.4f, white, 0.0f);
  ui_element({.layout = {.sizing = cod_fixed2(9.4f * s, 16.9f * s)},
              .floating = cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){o.x + 3.3f * s, o.y + 3.1f * s}),
              .background_color = white,
              .corner_radius = {.top_left = 0.9f * s, .top_right = 0.9f * s, .bottom_left = 2.6f * s, .bottom_right = 2.6f * s},
              .shadow = {.color = cod_rgba(0, 0, 0, 0.95f), .blur = 4.0f * k},
              .pointer_capture_mode = COD_PASS}) {}
  cod_shape_box(s, o, 4.6f, 6.2f, 6.8f, 1.2f, cod_rgba(0, 0, 0, 0.45f), 0.0f);
  cod_shape_box(s, o, 4.6f, 9.1f, 6.8f, 1.2f, cod_rgba(0, 0, 0, 0.45f), 0.0f);
}

hz_internal hz_noinline void cod_hud_draw_slot(const CodUi *u, u32 count, b32 frag, f32 k) {
  ui_element({.layout = {.child_gap = (u16)(8.0f * k + 0.5f), .child_alignment = {.y = UI_ALIGN_Y_CENTER}},
              .opacity = opt_some(f32, count == 0 ? 0.34f : 0.9f),
              .pointer_capture_mode = COD_PASS}) {
    ui_element({.layout = {.sizing = cod_fixed2(13.0f * k, 16.5f * k)}, .pointer_capture_mode = COD_PASS}) {
      if (frag)
        cod_hud_draw_frag(NULL, k);
      else
        cod_hud_draw_flash(k);
    }
    ui_element({.layout = {.sizing = {.width = ui_sizing_fixed(7.0f * k)}}, .pointer_capture_mode = COD_PASS}) {
      ui_text(cod_str_u32(count), cod_o1(cod_text(u, COD_FONT_BODY, 11.0f * k, UI_FONT_SEMIBOLD, COD_INK2, 0.96f), k));
    }
  }
}

hz_internal hz_noinline void cod_hud_draw_ammo_head(const CodUi *u, const CodHudModel *m, f32 k) {
  ui_element({.layout = {.child_gap = (u16)(8.0f * k + 0.5f), .child_alignment = {.y = UI_ALIGN_Y_CENTER}, .margin = {.bottom = (u16)(4.4f * k + 0.5f)}},
              .pointer_capture_mode = COD_PASS}) {
    ui_element({.layout = {.padding = {(u16)(4.0f * k + 0.5f), (u16)(4.0f * k + 0.5f), (u16)(1.5f * k + 0.5f), (u16)(1.5f * k + 0.5f)}},
                .background_color = cod_rgba(6, 10, 13, 0.34f),
                .border = {.color = COD_HAIR, .width = ui_border_outside(1)},
                .pointer_capture_mode = COD_PASS}) {
      ui_text(cod_ui_str(cod_hud_fire_mode_label(m->fire_mode)),
              cod_sh_hard(cod_text(u, COD_FONT_BODY, 9.5f * k, UI_FONT_SEMIBOLD, COD_INK2, 0.2f * 9.5f * k)));
    }
    ui_text(cod_ui_str(cod_hud_weapon_label(m->weapon)),
            cod_o1(cod_text(u, COD_FONT_BODY, 12.5f * k, UI_FONT_SEMIBOLD, COD_INK, 0.22f * 12.5f * k), k));
  }
}

hz_internal hz_noinline void cod_hud_draw_ammo_count(const CodUi *u, const CodHudFx *fx, const CodHudModel *m, f32 k) {
  f32 frac = (f32)m->ammo / (f32)(m->mag_size > 0 ? m->mag_size : 30);
  HzUIColor cur = m->ammo == 0 ? COD_RED : (m->ammo > 0 && frac <= 0.34f) ? COD_AMBER : COD_INK;
  f32 p = 1.0f - 0.075f * ease_quad_out(fx->punch);
  ui_element({.layout = {.child_gap = (u16)(8.0f * 0.55f * k + 0.5f), .child_alignment = {.y = UI_ALIGN_Y_BASELINE}},
              .pointer_capture_mode = COD_PASS}) {
    ui_element({.transform = opt_some(HzUITransform, ((HzUITransform){.scale = {p, p}})), .pointer_capture_mode = COD_PASS}) {
      HzUITextConfig c = cod_text(u, COD_FONT_DISPLAY, 56.0f * k, UI_FONT_BOLD, cur, 0.02f * 56.0f * k);
      c.lineHeight = 56.0f * k;
      ui_text(cod_str_u32(m->ammo), cod_o2_bloom(c, k, 16.0f, 0.55f));
    }
    HzUITextConfig sep = cod_text(u, COD_FONT_BODY, 20.0f * k, UI_FONT_REGULAR, COD_INK3, 0.96f);
    sep.lineHeight = 20.0f * k;
    ui_text(ui_string("/"), cod_o1(sep, k));
    HzUITextConfig res = cod_text(u, COD_FONT_DISPLAY, 24.0f * k, UI_FONT_SEMIBOLD, COD_INK2, 0.96f);
    res.lineHeight = 24.0f * k;
    ui_text(cod_str_u32(m->reserve), cod_o1(res, k));
  }
}

// one pip per round of the magazine in the gun; while reloading the old magazine leaves and a fresh one seats
hz_internal hz_noinline void cod_hud_draw_pips(const CodHudModel *m, f32 k) {
  u32 mag = m->mag_size > 0 ? m->mag_size : 30;
  u32 count = MIN(COD_HUD_MAX_PIPS, mag);
  f32 ammo = (f32)m->ammo;
  if (m->reloading) {
    f32 after = (f32)MIN(mag, m->ammo + m->reserve);
    f32 p = m_saturatef(m->reload_progress);
    ammo = p < 0.45f ? (f32)m->ammo * (1.0f - p / 0.45f) : after * ((p - 0.45f) / 0.55f);
  }
  u32 filled = ammo <= 0.001f ? 0 : (u32)m_maxf(1.0f, m_roundf(ammo / (f32)mag * (f32)count));
  ui_element({.layout = {.child_gap = (u16)(1.6f * k + 0.5f), .margin = {.top = (u16)(4.4f * k + 0.5f)}}, .pointer_capture_mode = COD_PASS}) {
    for (u32 i = 0; i < count; i++) {
      b32 on = i < filled;
      b32 warn = on && !m->reloading && (f32)filled / (f32)count <= 0.34f;
      HzUIColor color = on ? (warn ? COD_AMBER : COD_INK) : cod_rgba(6, 10, 13, 0.62f);
      ui_element({.layout = {.sizing = cod_fixed2(2.6f * k, 10.0f * k)},
                  .background_color = color,
                  .shadow = on ? (HzUIShadow){.color = cod_rgba(4, 8, 11, 0.75f), .spread = 1.0f}
                               : (HzUIShadow){.color = cod_rgba(255, 255, 255, 0.07f), .spread = 1.0f, .inset = true},
                  .shadows = {on ? (HzUIShadow){0} : (HzUIShadow){.color = cod_rgba(0, 0, 0, 0.5f), .spread = 1.0f}},
                  .pointer_capture_mode = COD_PASS}) {}
    }
  }
}

hz_internal hz_noinline void cod_hud_draw_reload(const CodUi *u, const CodHudModel *m, f32 k, b32 touch) {
  b32 reloading = m->reloading;
  if (!reloading && m->ammo != 0)
    return;
  f32 opacity = 1.0f;
  if (!reloading)
    opacity = 0.55f + 0.45f * m_absf(m_sinf(m->time * 3.8f));
  ui_element({.layout = {.margin = {.top = (u16)(6.4f * k + 0.5f)}}, .opacity = opt_some(f32, opacity), .pointer_capture_mode = COD_PASS}) {
    ui_text(reloading ? ui_string("RELOADING") : touch ? ui_string("TAP RELOAD") : ui_string("PRESS R TO RELOAD"),
            cod_o1(cod_text(u, COD_FONT_BODY, 10.5f * k, UI_FONT_SEMIBOLD, COD_AMBER, 0.28f * 10.5f * k), k));
  }
  if (reloading) {
    ui_element({.layout = {.sizing = cod_fixed2(86.0f * k, 2.5f * k), .layout_direction = UI_STACK, .margin = {.top = (u16)(3.2f * k + 0.5f)}},
                .background_color = cod_rgba(6, 10, 13, 0.7f),
                .shadow = {.color = cod_rgba(0, 0, 0, 0.4f), .spread = 1.0f},
                .clip = {.horizontal = true, .vertical = true},
                .pointer_capture_mode = COD_PASS}) {
      ui_element({.layout = {.sizing = {ui_sizing_grow(0), ui_sizing_grow(0)}},
                  .background_color = COD_AMBER,
                  .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {m_saturatef(m->reload_progress), 1.0f}, .origin = {0.0001f, 0.5f}})),
                  .pointer_capture_mode = COD_PASS}) {}
    }
  }
}

hz_internal hz_noinline void cod_hud_draw_ammo(const CodUi *u, const CodHudFx *fx, const CodHudModel *m, const CodHudView *v) {
  f32 k = v->k;
  f32 bottom = -(COD_PAD * k + v->safe.bottom);
  HzUIFloating at = v->touch ? cod_float_root_at(UI_ATTACH_POINT_LEFT_BOTTOM, UI_ATTACH_POINT_CENTER_BOTTOM, (v2){COD_TOUCH_STATUS_GAP, bottom}, COD_Z_CHROME)
                             : cod_float_root(UI_ATTACH_POINT_RIGHT_BOTTOM, (v2){-(COD_PAD * k + v->safe.right), bottom}, COD_Z_CHROME);
  ui_element({.layout = {.sizing = {.width = ui_sizing_fixed(168.0f * k)},
                         .layout_direction = UI_TOP_TO_BOTTOM,
                         .child_alignment = {.x = v->touch ? UI_ALIGN_X_LEFT : UI_ALIGN_X_RIGHT}},
              .floating = at,
              .pointer_capture_mode = COD_PASS}) {
    ui_element({.layout = {.child_gap = (u16)(16.0f * k + 0.5f), .margin = {.bottom = (u16)(5.6f * k + 0.5f)}}, .pointer_capture_mode = COD_PASS}) {
      cod_hud_draw_slot(u, m->lethal, true, k);
      cod_hud_draw_slot(u, m->tactical, false, k);
    }
    cod_hud_draw_ammo_head(u, m, k);
    cod_hud_draw_ammo_count(u, fx, m, k);
    cod_hud_draw_pips(m, k);
    cod_hud_draw_reload(u, m, k, v->touch);
  }
}

// ---- killfeed (top right) ----

// the assault rifle glyph of a killfeed row (viewBox 28 x 11 in a 31 x 12 box)
hz_internal hz_noinline void cod_hud_draw_rifle(f32 k) {
  f32 s = 1.0909f * k;
  v2 o = {0.223f * k, 0.0f};
  HzUIColor c = cod_rgba(240, 246, 250, 0.9f);
  cod_shape_poly4(s, o, (v2){0.5f, 4.2f}, (v2){6.0f, 4.2f}, (v2){6.0f, 8.0f}, (v2){2.2f, 8.0f}, c);
  cod_shape_box(s, o, 6.0f, 3.6f, 8.2f, 3.4f, c, 0.0f);
  cod_shape_box(s, o, 14.2f, 4.2f, 6.4f, 2.1f, c, 0.0f);
  cod_shape_box(s, o, 20.6f, 4.6f, 6.9f, 1.2f, c, 0.0f);
  cod_shape_box(s, o, 23.6f, 2.6f, 1.1f, 2.0f, c, 0.0f);
  cod_shape_poly4(s, o, (v2){9.0f, 7.0f}, (v2){12.6f, 7.0f}, (v2){11.9f, 11.0f}, (v2){9.7f, 11.0f}, c);
  cod_shape_poly4(s, o, (v2){13.0f, 7.0f}, (v2){15.0f, 7.0f}, (v2){14.1f, 10.2f}, (v2){12.5f, 10.2f}, c);
}

// the headshot skull (viewBox 11 x 11 in a 12 x 12 box)
hz_internal hz_noinline void cod_hud_draw_skull(f32 k) {
  f32 s = 1.0909f * k;
  v2 o = {0, 0};
  HzUIColor gold = cod_rgba(255, 194, 71, 0.95f);
  HzUIColor dark = cod_rgba(10, 12, 14, 0.9f);
  cod_shape_box(s, o, 1.4f, 0.8f, 8.2f, 8.0f, gold, 4.1f);
  cod_shape_box(s, o, 3.0f, 6.0f, 5.1f, 3.1f, gold, 0.0f);
  cod_shape_box(s, o, 2.75f, 3.45f, 2.3f, 2.3f, dark, 1.15f);
  cod_shape_box(s, o, 5.95f, 3.45f, 2.3f, 2.3f, dark, 1.15f);
  cod_shape_box(s, o, 5.05f, 6.1f, 0.9f, 1.3f, dark, 0.0f);
  cod_shape_box(s, o, 3.4f, 9.2f, 1.4f, 1.5f, gold, 0.0f);
  cod_shape_box(s, o, 6.2f, 9.2f, 1.4f, 1.5f, gold, 0.0f);
}

hz_internal void cod_hud_kill_push(CodHudFx *fx, const CodHudKillRow *row) {
  CodHudKillItem *best = &fx->kills[0];
  b32 found = false;
  for (u32 i = 0; i < COD_HUD_KILLFEED; i++) {
    CodHudKillItem *it = &fx->kills[(fx->kills_next + i) % COD_HUD_KILLFEED];
    if (!it->pool.alive) {
      best = it;
      found = true;
      fx->kills_next = (fx->kills_next + i + 1) % COD_HUD_KILLFEED;
      break;
    }
  }
  if (!found) {
    // oldest first: the row furthest through its life
    f32 oldest = -1.0f;
    for (u32 i = 0; i < COD_HUD_KILLFEED; i++) {
      f32 age = fx->kills[i].pool.t / fx->kills[i].pool.life;
      if (age > oldest) {
        oldest = age;
        best = &fx->kills[i];
      }
    }
  }
  best->pool = (CodHudPoolItem){.alive = true, .t = 0.0f, .life = 5.6f, .serial = ++fx->kill_serial};
  best->row = *row;
}

hz_internal void cod_hud_kills_update(CodHudFx *fx, f32 dt) {
  for (u32 i = 0; i < COD_HUD_KILLFEED; i++) {
    CodHudPoolItem *p = &fx->kills[i].pool;
    if (!p->alive)
      continue;
    p->t += dt;
    if (p->t >= p->life)
      p->alive = false;
  }
}

hz_internal hz_noinline void cod_hud_draw_kill_row(const CodUi *u, const CodHudKillItem *it, f32 k, f32 vis) {
  const CodHudKillRow *r = &it->row;
  f32 t = it->pool.t, life = it->pool.life;
  f32 in_t = m_saturatef(t / 0.16f);
  f32 out_t = m_saturatef((t - (life - 0.45f)) / 0.45f);
  f32 x = (1.0f - ease_quint_out(in_t)) * 26.0f;
  f32 a = ease_quad_out(in_t) * (1.0f - ease_quad_in(out_t));
  HzUIColor attacker = r->mine ? COD_WHITE : r->attacker_enemy ? COD_ENEMY : COD_FRIEND;
  HzUIColor victim = r->attacker_enemy ? COD_FRIEND : COD_ENEMY;
  CodStops flat = {.n = 0};
  CodStops fade_in = cod_stops(3, 0.0f, 0.0f, 0.22f, 1.0f, 1.0f, 1.0f, 0, 0, 0, 0);
  HzUIColor scrim = r->mine ? (HzUIColor){26, 17, 3, 255} : (HzUIColor){5, 9, 12, 255};
  f32 scrim_a = r->mine ? 0.66f : 0.58f;
  u16 edge = (u16)m_maxf(1.0f, m_roundf(2.0f * k));
  ui_element({.layout = {.layout_direction = UI_STACK},
              .border = {.color = r->mine ? COD_AMBER : cod_rgba(255, 255, 255, 0.18f), .width = {.right = edge}},
              .transform = opt_some(HzUITransform, ((HzUITransform){.translate = {x, 0.0f}})),
              .opacity = opt_some(f32, a),
              .pointer_capture_mode = COD_PASS}) {
    cod_band_element(u, (HzUISizing){ui_sizing_grow(0), ui_sizing_grow(0)}, (HzUIFloating){0}, scrim, scrim_a * a * vis, &flat, &flat, &fade_in);
    ui_element({.layout = {.padding = {(u16)(6.0f * k + 0.5f), (u16)(6.0f * k + 0.5f), (u16)(3.2f * k + 0.5f), (u16)(3.2f * k + 0.5f)},
                           .child_gap = (u16)(6.4f * k + 0.5f),
                           .child_alignment = {.y = UI_ALIGN_Y_CENTER}},
                .pointer_capture_mode = COD_PASS}) {
      HzUITextConfig c = cod_text(u, COD_FONT_BODY, 13.5f * k, UI_FONT_SEMIBOLD, attacker, 0.09f * 13.5f * k);
      ui_text(cod_ui_str(r->attacker.value), cod_o1(c, k));
      ui_element({.layout = {.child_gap = (u16)(3.2f * k + 0.5f), .child_alignment = {.y = UI_ALIGN_Y_CENTER}},
                  .opacity = opt_some(f32, 0.9f),
                  .pointer_capture_mode = COD_PASS}) {
        if (r->headshot)
          ui_element({.layout = {.sizing = cod_fixed2(12.0f * k, 12.0f * k)}, .pointer_capture_mode = COD_PASS}) { cod_hud_draw_skull(k); }
        ui_element({.layout = {.sizing = cod_fixed2(31.0f * k, 12.0f * k)}, .pointer_capture_mode = COD_PASS}) { cod_hud_draw_rifle(k); }
      }
      c.textColor = victim;
      ui_text(cod_ui_str(r->victim.value), cod_o1(c, k));
    }
  }
}

hz_internal hz_noinline void cod_hud_draw_killfeed(const CodUi *u, const CodHudFx *fx, const CodHudView *v) {
  f32 k = v->k, vis = v->vis;
  ui_element({.layout = {.layout_direction = UI_TOP_TO_BOTTOM, .child_gap = (u16)(4.4f * k + 0.5f), .child_alignment = {.x = UI_ALIGN_X_RIGHT}},
              .floating = cod_float_root(UI_ATTACH_POINT_RIGHT_TOP,
                                         (v2){-(COD_PAD * k + v->safe.right), (COD_PAD + COD_FPS_H + 12.0f) * k + v->safe.top}, COD_Z_CHROME),
              .pointer_capture_mode = COD_PASS}) {
    // newest on top
    u32 last = ~0u;
    for (u32 n = 0; n < COD_HUD_KILLFEED; n++) {
      const CodHudKillItem *pick = NULL;
      for (u32 i = 0; i < COD_HUD_KILLFEED; i++) {
        const CodHudKillItem *it = &fx->kills[i];
        if (!it->pool.alive || (last != ~0u && it->pool.serial >= last))
          continue;
        if (!pick || it->pool.serial > pick->pool.serial)
          pick = it;
      }
      if (!pick)
        break;
      last = pick->pool.serial;
      cod_hud_draw_kill_row(u, pick, k, vis);
    }
  }
}
