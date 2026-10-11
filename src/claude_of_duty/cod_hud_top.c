// Claude of Duty HUD, the screen's frame: compass, match bar, minimap, banner, prompt and the world markers.

#define COD_COMPASS_W 470.0f
#define COD_COMPASS_H 41.0f
#define COD_COMPASS_SPAN 120.0f
#define COD_COMPASS_PPD (COD_COMPASS_W / COD_COMPASS_SPAN)
#define COD_MAP_SIZE 178.0f
#define COD_MAP_VIEW_M 60.0f

hz_internal HzUIColor cod_cardinal_color(b32 sub) { return sub ? (HzUIColor){233, 243, 249, 229.5f} : COD_WHITE; }

// the css mask-image linear-gradient(to right, transparent, #000 16%, #000 84%, transparent) at a strip fraction
hz_internal f32 cod_compass_mask(f32 f) { return m_saturatef(f / 0.16f) * m_saturatef((1.0f - f) / 0.16f); }

hz_internal const char *cod_cardinal_name(u32 deg) {
  switch (deg) {
  case 0: return "N";
  case 45: return "NE";
  case 90: return "E";
  case 135: return "SE";
  case 180: return "S";
  case 225: return "SW";
  case 270: return "W";
  case 315: return "NW";
  default: return NULL;
  }
}

// one tick and its cardinal at a strip x (px from the strip's left edge)
hz_internal hz_noinline void cod_hud_draw_tick(const CodUi *u, u32 deg, f32 x, f32 k, f32 mask) {
  b32 major = deg % 15 == 0;
  f32 w = major ? 1.5f * k : 1.0f, h = (major ? 7.5f : 4.0f) * k;
  ui_element({.layout = {.sizing = cod_fixed2(w, h)},
              .floating = cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){x, 19.0f * k}),
              .background_color = major ? cod_rgba(255, 255, 255, 0.95f) : cod_rgba(255, 255, 255, 0.7f),
              .shadow = {.color = cod_rgba(4, 8, 11, 0.6f), .spread = 1.0f},
              .shadows = {{.color = cod_rgba(0, 0, 0, 0.9f), .blur = 2.0f * k}},
              .opacity = opt_some(f32, mask),
              .pointer_capture_mode = COD_PASS}) {}
  const char *name = cod_cardinal_name(deg % 360);
  if (!name)
    return;
  b32 sub = name[1] != 0;
  f32 size = (sub ? 10.0f : 13.5f) * k;
  ui_element({.floating = cod_float_parent(UI_ATTACH_POINT_CENTER_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){x, (sub ? 3.5f : 1.0f) * k}),
              .opacity = opt_some(f32, mask),
              .pointer_capture_mode = COD_PASS}) {
    ui_text(cod_ui_str(name), cod_o1(cod_text(u, COD_FONT_BODY, size, UI_FONT_BOLD, cod_cardinal_color(sub), 0.1f * size), k));
  }
}

hz_internal hz_noinline void cod_hud_draw_compass(const CodUi *u, const CodHudModel *m, const CodHudView *v) {
  f32 k = v->k, vis = v->vis;
  f32 heading = m_wrap_pi(m_radians(m->heading_deg));
  f32 h = m_degrees(heading);
  if (h < 0.0f)
    h += 360.0f;
  CodStops flat = {.n = 0};
  CodStops scrim_v = cod_stops(5, 0.0f, 0.0f, 0.2f, 0.45f, 0.66f, 0.45f, 0.88f, 0.2f, 1.0f, 0.0f);
  CodStops scrim_h = cod_stops(4, 0.0f, 0.0f, 0.2f, 1.0f, 0.8f, 1.0f, 1.0f, 0.0f, 0, 0);
  CodStops outer = cod_stops(4, 0.0f, 0.0f, 0.16f, 1.0f, 0.84f, 1.0f, 1.0f, 0.0f, 0, 0);
  CodStops base_h = cod_stops(3, 0.0f, 0.0f, 0.5f, 0.4f, 1.0f, 0.0f, 0, 0, 0, 0);
  ui_element({.layout = {.sizing = cod_fixed2(COD_COMPASS_W * k, COD_COMPASS_H * k)},
              .floating = cod_float_root(UI_ATTACH_POINT_CENTER_TOP, (v2){0, COD_PAD * 0.7f * k + v->safe.top}, COD_Z_CHROME),
              .pointer_capture_mode = COD_PASS}) {
    HzUIFloating at0 = cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){0, 0});
    cod_band_element(u, cod_fixed2(COD_COMPASS_W * k, COD_COMPASS_H * k), at0, (HzUIColor){3, 6, 9, 255}, vis, &scrim_v, &scrim_h, &outer);
    // the strip: ticks every 5 degrees across two revolutions; only those inside the 120 degree span are visible
    for (i32 a = 0; a < 720; a += 5) {
      f32 rel = (f32)a - 360.0f - h;
      if (rel < -62.0f || rel > 62.0f)
        continue;
      f32 x = COD_COMPASS_W * 0.5f * k + rel * COD_COMPASS_PPD * k;
      f32 mask = cod_compass_mask(x / (COD_COMPASS_W * k));
      if (mask > 0.0f)
        cod_hud_draw_tick(u, (u32)a, x, k, mask);
    }
    // the base line and its dark seat, both under the outer mask
    cod_band_element(u, cod_fixed2(COD_COMPASS_W * k, 1.0f),
                     cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){0, 19.0f * k}),
                     (HzUIColor){4, 8, 11, 255}, 0.5f * vis, &flat, &flat, &outer);
    cod_band_element(u, cod_fixed2(COD_COMPASS_W * k, 1.0f),
                     cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){0, 18.0f * k}),
                     (HzUIColor){255, 255, 255, 255}, vis, &flat, &base_h, &outer);
    // the caret: an amber triangle pointing down
    f32 cw = 9.0f * k, ch = 5.5f * k;
    ui_element({.layout = {.sizing = cod_fixed2(cw, ch)},
                .floating = cod_float_parent(UI_ATTACH_POINT_CENTER_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){COD_COMPASS_W * 0.5f * k, 13.5f * k}),
                .background_color = cod_rgba(0, 0, 0, 0.6f),
                .clip_path = {.count = 3, .points = {{.offset = {0, 0}}, {.offset = {cw, 0}}, {.offset = {cw * 0.5f, ch}}}},
                .pointer_capture_mode = COD_PASS}) {}
    ui_element({.layout = {.sizing = cod_fixed2(cw, ch)},
                .floating = cod_float_parent(UI_ATTACH_POINT_CENTER_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){COD_COMPASS_W * 0.5f * k, 12.5f * k}),
                .background_color = COD_AMBER,
                .clip_path = {.count = 3, .points = {{.offset = {0, 0}}, {.offset = {cw, 0}}, {.offset = {cw * 0.5f, ch}}}},
                .pointer_capture_mode = COD_PASS}) {}
    // objective markers on the strip, clamped to its ends
    f32 half = COD_COMPASS_W * 0.5f * k;
    for (u32 i = 0; i < COD_HUD_COMPASS_PINS && i < COD_HUD_PINS; i++) {
      const CodHudPinView *p = &m->pins[i];
      if (!p->active)
        continue;
      f32 bearing = m_degrees(hud_bearing(m->player_xz, p->world_xz));
      f32 rel = bearing - h;
      while (rel > 180.0f)
        rel -= 360.0f;
      while (rel < -180.0f)
        rel += 360.0f;
      f32 px = m_clampf(rel * COD_COMPASS_PPD * k, -half + 8.0f * k, half - 8.0f * k);
      f32 mask = cod_compass_mask((half + px) / (COD_COMPASS_W * k));
      f32 s = 13.0f * k;
      ui_element({.layout = {.sizing = cod_fixed2(s, s), .child_alignment = {.x = UI_ALIGN_X_CENTER, .y = UI_ALIGN_Y_CENTER}},
                  .floating = cod_float_parent(UI_ATTACH_POINT_CENTER_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){half + px, 28.0f * k}),
                  .background_color = COD_CYAN,
                  .shadow = {.color = cod_rgba(0, 0, 0, 0.8f), .offset_y = 1.0f, .blur = 2.0f},
                  .opacity = opt_some(f32, mask * (m_absf(rel) > COD_COMPASS_SPAN * 0.5f ? 0.45f : 1.0f)),
                  .pointer_capture_mode = COD_PASS}) {
        ui_text(cod_ui_str(p->label.value), cod_text(u, COD_FONT_BODY, 9.5f * k, UI_FONT_SEMIBOLD, (HzUIColor){8, 22, 28, 255}, 0.06f * 9.5f * k));
      }
    }
  }
}

// the scoreline under the compass
hz_internal hz_noinline void cod_hud_draw_match(const CodUi *u, const CodHudModel *m, const CodHudView *v) {
  f32 k = v->k;
  f32 size = 11.0f * k, ls = 0.18f * size;
  u32 secs = (u32)m_maxf(0.0f, m->time_left);
  String32Bytes clock = {0};
  cstr_format(clock.value, sizeof(clock.value), "%:%", fmt_u32(secs / 60), fmt_u32_pad(secs % 60, 2));
  clock.len = (u32)cstr_len(clock.value);
  HzUITextConfig base = cod_o1(cod_text(u, COD_FONT_BODY, size, UI_FONT_SEMIBOLD, COD_INK2, ls), k);
  HzUITextConfig clk = base;
  clk.textColor = COD_INK;
  HzUITextConfig num = cod_o1(cod_text(u, COD_FONT_DISPLAY, 19.0f * k, UI_FONT_BOLD, COD_FRIEND, 0.04f * 19.0f * k), k);
  ui_element({.layout = {.child_gap = (u16)(10.0f * k + 0.5f), .child_alignment = {.y = UI_ALIGN_Y_CENTER}},
              .floating = cod_float_root(UI_ATTACH_POINT_CENTER_TOP, (v2){0, (COD_PAD * 0.7f + 45.0f) * k + v->safe.top}, COD_Z_CHROME),
              .pointer_capture_mode = COD_PASS}) {
    ui_text(cod_str_u32(m->score_us), num);
    ui_element({.layout = {.sizing = cod_fixed2(1.0f, 11.0f * k)}, .background_color = (HzUIColor){255, 255, 255, 38.25f}, .pointer_capture_mode = COD_PASS}) {}
    ui_text(ui_string("TDM"), base);
    ui_text(cod_ui_str(clock.value), clk);
    ui_element({.layout = {.sizing = cod_fixed2(1.0f, 11.0f * k)}, .background_color = (HzUIColor){255, 255, 255, 38.25f}, .pointer_capture_mode = COD_PASS}) {}
    num.textColor = COD_ENEMY;
    ui_text(cod_str_u32(m->score_them), num);
  }
}

// ---- frame counter ----

// touch: the pause button's corner, kept clear
#define COD_TOUCH_PAUSE_CLEAR 62.0f

// fps and frame time averaged over half-second windows, sized to read in a downscaled capture; drawn over the loading screen too
hz_internal hz_noinline void cod_hud_draw_fps(CodUi *u, f32 frame_dt, const CodHudView *v) {
  f32 k = v->k;
  u->fps_window_s += frame_dt;
  u->fps_frames++;
  if (u->fps_window_s >= 0.5f || u->fps_shown == 0.0f) {
    u->fps_shown = u->fps_window_s > 0.0f ? (f32)u->fps_frames / u->fps_window_s : 0.0f;
    u->frame_ms_shown = u->fps_window_s * 1000.0f / (f32)u->fps_frames;
    u->fps_window_s = 0.0f;
    u->fps_frames = 0;
  }
  u32 fps = (u32)(u->fps_shown + 0.5f);
  HzUIColor tone = fps >= 60 ? COD_INK : fps >= 30 ? COD_AMBER : COD_RED;
  HzUIColor accent = fps >= 60 ? COD_CYAN : tone;
  String32Bytes ms = {0};
  cstr_format(ms.value, sizeof(ms.value), "% MS", fmt_f32_p(u->frame_ms_shown, 1));
  u16 pad_x = (u16)(16.0f * k + 0.5f);
  ui_element({.layout = {.sizing = cod_fixed2(COD_FPS_W * k, COD_FPS_H * k),
                         .padding = {pad_x, pad_x, (u16)(8.0f * k + 0.5f), (u16)(10.0f * k + 0.5f)},
                         .layout_direction = UI_TOP_TO_BOTTOM,
                         .child_alignment = {.x = UI_ALIGN_X_RIGHT, .y = UI_ALIGN_Y_CENTER}},
              .floating = cod_float_root(UI_ATTACH_POINT_RIGHT_TOP,
                                         (v2){-(COD_PAD * k + v->safe.right + (v->touch ? COD_TOUCH_PAUSE_CLEAR : 0.0f)), COD_PAD * k + v->safe.top},
                                         COD_Z_LOADING + 1),
              .background_color = cod_rgba(5, 9, 12, 0.72f),
              .corner_radius = ui_corner_radius(4.0f * k),
              .border = {.color = accent, .width = {.right = (u16)(3.0f * k + 0.5f)}},
              .shadow = {.color = cod_rgba(0, 0, 0, 0.3f), .offset_y = 2.0f * k, .blur = 10.0f * k},
              .pointer_capture_mode = COD_PASS}) {
    ui_element({.layout = {.child_gap = (u16)(8.0f * k + 0.5f), .child_alignment = {.y = UI_ALIGN_Y_BOTTOM}}, .pointer_capture_mode = COD_PASS}) {
      ui_text(cod_str_u32(fps), cod_o2(cod_text(u, COD_FONT_DISPLAY, 66.0f * k, UI_FONT_BOLD, tone, 0.02f * 66.0f * k), k));
      ui_element({.layout = {.padding = {.bottom = (u16)(10.0f * k + 0.5f)}}, .pointer_capture_mode = COD_PASS}) {
        ui_text(ui_string("FPS"), cod_o1(cod_text(u, COD_FONT_DISPLAY, 22.0f * k, UI_FONT_BOLD, COD_INK2, 0.2f * 22.0f * k), k));
      }
    }
    ui_text(cod_ui_str(ms.value), cod_o1(cod_text(u, COD_FONT_MONO, 22.0f * k, UI_FONT_SEMIBOLD, tone, 0.04f * 22.0f * k), k));
  }
}

// ---- minimap ----

hz_internal void cod_hud_fill_map(ShaderCodUiMinimapMapParams *p, const CodHudModel *m, const CodWorld *world, f32 k, f32 vis) {
  f32 s = COD_MAP_SIZE * k;
  p->view = (vec4){s, COD_MAP_VIEW_M, m->player_xz.x, m->player_xz.y};
  p->cone = (vec4){m_radians(m->heading_deg), m_radians(m->fov_deg * 0.5f), k, vis};
  p->level = (vec4){m_cosf(COD_LEVEL_YAW), m_sinf(COD_LEVEL_YAW), COD_LEVEL_TX, COD_LEVEL_TZ};
  u32 nb = MIN(world->building_count, COD_WORLD_MAX_BUILDINGS);
  u32 nl = MIN(m->blip_count, COD_HUD_BLIPS);
  u32 na = MIN(world->alley_count, (u32)ARRAY_SIZE(p->alleys));
  p->counts = (vec4){(f32)nl, (f32)nb, (f32)na, 4.0f * k};
  p->street = (vec4){world->street.x - 0.1f, world->street.y, world->street.z, 0.0f};
  for (u32 i = 0; i < na; i++)
    p->alleys[i] = (vec4){world->alleys[i].x, world->alleys[i].y, world->alleys[i].z, world->alleys[i].w};
  for (u32 i = 0; i < nb; i++) {
    const CodBuildingInfo *b = &world->buildings[i];
    f32 cx = b->level_rect.x, cz = b->level_rect.y, w = b->level_rect.z, d = b->level_rect.w;
    p->bld[2 * i] = (vec4){cx - w * 0.5f, cz - d * 0.5f, cx + w * 0.5f, cz + d * 0.5f};
    p->bld[2 * i + 1] = (vec4){m_saturatef(((f32)b->floors - 1.0f) / 3.0f), 0, 0, 0};
  }
  for (u32 i = 0; i < nl; i++)
    p->blips[i] = (vec4){m->blips[i].x, m->blips[i].z, m_radians(m->blips[i].heading_deg), m->blips[i].friendly ? 1.0f : 0.0f};
}

hz_internal hz_noinline void cod_hud_draw_map_corner(f32 k, b32 right, b32 bottom) {
  f32 s = 9.0f * k;
  HzUIColor c = cod_rgba(255, 255, 255, 0.32f);
  f32 x = right ? COD_MAP_SIZE * k + 1.0f * k - s : -1.0f * k;
  f32 y = bottom ? COD_MAP_SIZE * k + 1.0f * k - s : -1.0f * k;
  f32 hy = bottom ? y + s - 1.0f : y;
  f32 vx = right ? x + s - 1.0f : x;
  ui_element({.layout = {.sizing = cod_fixed2(s, 1.0f)},
              .floating = cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){x, hy}),
              .background_color = c,
              .pointer_capture_mode = COD_PASS}) {}
  ui_element({.layout = {.sizing = cod_fixed2(1.0f, s)},
              .floating = cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){vx, y}),
              .background_color = c,
              .pointer_capture_mode = COD_PASS}) {}
}

hz_internal hz_noinline void cod_hud_draw_map_pip(const CodUi *u, const CodHudPinView *p, const CodHudModel *m, f32 k) {
  f32 s = COD_MAP_SIZE * k, ppm = s / COD_MAP_VIEW_M, r = 6.0f * k;
  f32 dx = m_clampf((p->world_xz.x - m->player_xz.x) * ppm + s * 0.5f, r + 1.0f, s - r - 1.0f);
  f32 dy = m_clampf((p->world_xz.y - m->player_xz.y) * ppm + s * 0.5f, r + 1.0f, s - r - 1.0f);
  ui_element({.layout = {.sizing = cod_fixed2(2.0f * r, 2.0f * r), .child_alignment = {.x = UI_ALIGN_X_CENTER, .y = UI_ALIGN_Y_CENTER}},
              .floating = cod_float_parent(UI_ATTACH_POINT_CENTER_CENTER, UI_ATTACH_POINT_LEFT_TOP, (v2){dx, dy}),
              .background_color = cod_rgba(121, 210, 255, 0.94f),
              .border = {.color = cod_rgba(4, 14, 20, 0.8f), .width = {1, 1, 1, 1, 0}, .overlay = true},
              .pointer_capture_mode = COD_PASS}) {
    ui_text(cod_ui_str(p->label.value), cod_text(u, COD_FONT_BODY, 9.5f * k, UI_FONT_BOLD, (HzUIColor){6, 23, 31, 255}, 0.96f));
  }
}

hz_internal hz_noinline void cod_hud_draw_minimap(const CodUi *u, const CodHudModel *m, const CodWorld *world, const CodHudView *v) {
  f32 k = v->k, vis = v->vis;
  f32 s = COD_MAP_SIZE * k;
  Allocator ta = tctx_temp_allocator(NULL);
  ShaderCodUiMinimapMapParams *p = ALLOC(&ta, ShaderCodUiMinimapMapParams);
  cod_hud_fill_map(p, m, world, k, vis);
  ui_element({.layout = {.sizing = cod_fixed2(s, s)},
              .floating = cod_float_root(UI_ATTACH_POINT_LEFT_TOP, (v2){COD_PAD * k + v->safe.left, COD_PAD * k + v->safe.top}, COD_Z_CHROME),
              .pointer_capture_mode = COD_PASS}) {
    // the plate behind the frame
    ui_element({.layout = {.sizing = cod_fixed2(s + 14.0f * k, s + 14.0f * k)},
                .floating = cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){-7.0f * k, -7.0f * k}),
                .background_color = cod_rgba(4, 8, 11, 0.07f),
                .corner_radius = ui_corner_radius(10.0f * k),
                .shadow = {.color = cod_rgba(4, 8, 11, 0.05f), .blur = 16.0f * k, .spread = 6.0f * k},
                .pointer_capture_mode = COD_PASS}) {}
    ui_element({.layout = {.sizing = cod_fixed2(s, s)},
                .floating = cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){0, 0}),
                .corner_radius = ui_corner_radius(4.0f * k),
                .shadow = {.color = cod_rgba(0, 0, 0, 0.3f), .offset_y = 2.0f * k, .blur = 10.0f * k},
                .shader = {.pipeline = u->map_pipeline, .uniforms = p, .uniforms_size = sizeof(*p)},
                .pointer_capture_mode = COD_PASS}) {}
    for (u32 i = 0; i < COD_HUD_PINS; i++)
      if (m->pins[i].active)
        cod_hud_draw_map_pip(u, &m->pins[i], m, k);
    cod_hud_draw_map_corner(k, false, false);
    cod_hud_draw_map_corner(k, true, false);
    cod_hud_draw_map_corner(k, false, true);
    cod_hud_draw_map_corner(k, true, true);
    ui_element({.floating = cod_float_parent(UI_ATTACH_POINT_CENTER_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){s * 0.5f, -13.0f * k}),
                .pointer_capture_mode = COD_PASS}) {
      ui_text(ui_string("N"), cod_sh(cod_text(u, COD_FONT_BODY, 9.5f * k, UI_FONT_SEMIBOLD, COD_INK2, 0.2f * 9.5f * k), k));
    }
    ui_element({.layout = {.child_gap = (u16)(6.0f * k + 0.5f)},
                .floating = cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_BOTTOM, (v2){0, 4.0f * k}),
                .pointer_capture_mode = COD_PASS}) {
      HzUITextConfig c = cod_sh(cod_text(u, COD_FONT_BODY, 9.5f * k, UI_FONT_SEMIBOLD, COD_INK3, 0.2f * 9.5f * k), k);
      ui_text(ui_string("ZONE 07"), c);
      ui_text(ui_string("60M"), c);
    }
  }
}

// ---- banner ----

hz_internal hz_noinline void cod_hud_draw_banner(const CodUi *u, const HudMessages *msgs, const CodHudView *v) {
  HudMsgView mv;
  if (!hud_msg_view(msgs, HUD_MSG_BANNER, &mv))
    return;
  f32 k = v->k;
  f32 in_t = mv.phase == HUD_MSG_PHASE_ENTER ? mv.level : 1.0f;
  f32 a = mv.phase == HUD_MSG_PHASE_EXIT ? 1.0f - ease_quad_in(1.0f - mv.level) : ease_quad_out(in_t);
  f32 scale = 0.965f + 0.035f * cod_ease_out_back(in_t);
  CodStops v_prof = cod_stops(4, 0.0f, 0.0f, 0.2f, 0.6f, 0.8f, 0.6f, 1.0f, 0.0f, 0, 0);
  CodStops mask = cod_stops(4, 0.0f, 0.0f, 0.2f, 1.0f, 0.8f, 1.0f, 1.0f, 0.0f, 0, 0);
  CodStops flat = {.n = 0};
  f32 pad_x = 120.0f * k, pad_y = 16.0f * k;
  ui_element({.layout = {.layout_direction = UI_STACK},
              .floating = cod_float_root(UI_ATTACH_POINT_CENTER_CENTER, (v2){0, (0.31f - 0.5f) * v->h}, COD_Z_CHROME),
              .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {scale, scale}})),
              .opacity = opt_some(f32, a),
              .pointer_capture_mode = COD_PASS}) {
    // the band behind the type spans the padded box
    cod_band_element(u, (HzUISizing){ui_sizing_grow(0), ui_sizing_grow(0)}, (HzUIFloating){0}, (HzUIColor){4, 7, 10, 255}, a * v->vis,
                     &v_prof, &mask, &flat);
    ui_element({.layout = {.layout_direction = UI_TOP_TO_BOTTOM,
                           .child_alignment = {.x = UI_ALIGN_X_CENTER},
                           .padding = {(u16)pad_x, (u16)pad_x, (u16)pad_y, (u16)pad_y}},
                .pointer_capture_mode = COD_PASS}) {
      HzUITextConfig t = cod_text(u, COD_FONT_DISPLAY, 30.0f * k, UI_FONT_BOLD, COD_INK, 0.3f * 30.0f * k);
      ui_text(cod_ui_str(mv.msg->title.value), cod_o2(t, k));
      if (mv.msg->body.len) {
        ui_element({.layout = {.margin = {.top = (u16)(5.6f * k + 0.5f)}}, .pointer_capture_mode = COD_PASS}) {
          HzUITextConfig s = cod_text(u, COD_FONT_BODY, 12.0f * k, UI_FONT_BOLD, COD_AMBER, 0.3f * 12.0f * k);
          ui_text(cod_ui_str(mv.msg->body.value), cod_o1(s, k));
        }
      }
      ui_element({.layout = {.sizing = cod_fixed2(120.0f * k, 1.0f), .margin = {.top = (u16)(5.6f * k + 0.5f)}},
                  .gradient = {.kind = UI_GRADIENT_LINEAR,
                               .angle = 90.0f,
                               .stop_count = 3,
                               .stops = {{0.0f, {255, 255, 255, 0}}, {0.5f, {255, 255, 255, 127.5f}}, {1.0f, {255, 255, 255, 0}}}},
                  .pointer_capture_mode = COD_PASS}) {}
    }
  }
}

// ---- prompt ----

hz_internal void cod_hud_prompt_update(CodHudFx *fx, const CodHudModel *m, f32 dt) {
  if (m->prompt.active)
    fx->prompt_last = m->prompt;
  fx->prompt_shown = m_damp(fx->prompt_shown, m->prompt.active ? 1.0f : 0.0f, 18.0f, dt);
}

hz_internal hz_noinline void cod_hud_draw_prompt(const CodUi *u, const CodHudFx *fx, f32 k, f32 h) {
  f32 vis = fx->prompt_shown;
  if (vis < 0.005f || !fx->prompt_last.active)
    return;
  const CodHudPromptView *p = &fx->prompt_last;
  f32 y = (1.0f - ease_cubic_out(vis)) * 7.0f;
  ui_element({.layout = {.child_gap = (u16)(8.0f * k + 0.5f), .child_alignment = {.y = UI_ALIGN_Y_CENTER}},
              .floating = cod_float_root(UI_ATTACH_POINT_CENTER_CENTER, (v2){0, h * 0.08f + y}, COD_Z_CHROME),
              .opacity = opt_some(f32, vis),
              .pointer_capture_mode = COD_PASS}) {
    ui_element({.layout = {.sizing = {ui_sizing_fit(22.0f * k, 0.0f), ui_sizing_fixed(22.0f * k)},
                           .padding = {(u16)(4.8f * k + 0.5f), (u16)(4.8f * k + 0.5f), 0, 0},
                           .child_alignment = {.x = UI_ALIGN_X_CENTER, .y = UI_ALIGN_Y_CENTER}},
                .background_color = cod_rgba(8, 11, 14, 0.42f),
                .corner_radius = ui_corner_radius(2.0f * k),
                .border = {.color = cod_rgba(255, 255, 255, 0.55f), .width = {1, 1, 1, 1, 0}, .overlay = true},
                .shadow = {.color = cod_rgba(255, 255, 255, 0.14f), .offset_y = 1.0f, .inset = true},
                .shadows = {{.color = cod_rgba(0, 0, 0, 0.7f), .offset_y = 1.0f, .blur = 3.0f}},
                .pointer_capture_mode = COD_PASS}) {
      ui_text(cod_ui_str(p->key.value), cod_sh_hard(cod_text(u, COD_FONT_BODY, 11.0f * k, UI_FONT_SEMIBOLD, COD_INK, 0.06f * 11.0f * k)));
    }
    ui_element({.layout = {.layout_direction = UI_TOP_TO_BOTTOM}, .pointer_capture_mode = COD_PASS}) {
      ui_text(cod_ui_str(p->text.value), cod_sh(cod_text(u, COD_FONT_BODY, 12.0f * k, UI_FONT_SEMIBOLD, COD_INK, 0.2f * 12.0f * k), k));
      if (p->sub.len)
        ui_text(cod_ui_str(p->sub.value), cod_text(u, COD_FONT_BODY, 9.5f * k, UI_FONT_SEMIBOLD, COD_INK2, 0.2f * 9.5f * k));
      if (p->has_bar) {
        ui_element({.layout = {.sizing = {ui_sizing_grow(0), ui_sizing_fixed(1.5f * k)}, .layout_direction = UI_STACK, .margin = {.top = (u16)(4.0f * k + 0.5f)}},
                    .background_color = cod_rgba(255, 255, 255, 0.16f),
                    .pointer_capture_mode = COD_PASS}) {
          ui_element({.layout = {.sizing = {ui_sizing_grow(0), ui_sizing_grow(0)}},
                      .background_color = COD_AMBER,
                      .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {m_saturatef(p->progress), 1.0f}, .origin = {0.0001f, 0.5f}})),
                      .pointer_capture_mode = COD_PASS}) {}
        }
      }
    }
  }
}

// ---- world markers ----

hz_internal hz_noinline void cod_hud_draw_pin(const CodUi *u, const CodHudPinView *p, f32 k) {
  f32 fade = m_saturatef(1.15f - p->distance / 260.0f) * (p->offscreen ? 0.72f : 1.0f);
  String32Bytes dist = {0};
  if (p->distance < 10.0f)
    cstr_format(dist.value, sizeof(dist.value), "%M", fmt_f32_p(p->distance, 1));
  else
    cstr_format(dist.value, sizeof(dist.value), "%M", fmt_u32((u32)p->distance));
  f32 g = 16.0f * k;
  ui_element({.layout = {.sizing = {ui_sizing_fixed(40.0f * k), ui_sizing_fit(0)}, .layout_direction = UI_TOP_TO_BOTTOM, .child_alignment = {.x = UI_ALIGN_X_CENTER}},
              .floating = cod_float_root(UI_ATTACH_POINT_LEFT_TOP, (v2){p->screen.x - 20.0f * k, p->screen.y - 12.0f * k}, COD_Z_WORLD),
              .opacity = opt_some(f32, fade),
              .pointer_capture_mode = COD_PASS}) {
    ui_element({.layout = {.sizing = cod_fixed2(g, g), .layout_direction = UI_STACK, .child_alignment = {.x = UI_ALIGN_X_CENTER, .y = UI_ALIGN_Y_CENTER}},
                .pointer_capture_mode = COD_PASS}) {
      if (p->offscreen) {
        ui_element({.layout = {.sizing = cod_fixed2(12.8f * k, 11.5f * k)},
                    .floating = cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){1.6f * k, 1.5f * k}),
                    .background_color = cod_rgba(121, 210, 255, 0.95f),
                    .clip_path = {.count = 3, .points = {{.offset = {6.4f * k, 0}}, {.offset = {12.8f * k, 11.5f * k}}, {.offset = {0, 11.5f * k}}}},
                    .transform = opt_some(HzUITransform, ((HzUITransform){.rotation = p->angle_deg, .origin = {0.5f, 0.5f}})),
                    .pointer_capture_mode = COD_PASS}) {}
      } else {
        ui_element({.layout = {.sizing = cod_fixed2(9.6f * k, 9.6f * k)},
                    .floating = cod_float_parent(UI_ATTACH_POINT_CENTER_CENTER, UI_ATTACH_POINT_CENTER_CENTER, (v2){0, 0}),
                    .background_color = cod_rgba(121, 210, 255, 0.9f),
                    .border = {.color = cod_rgba(6, 20, 28, 0.75f), .width = {1, 1, 1, 1, 0}, .overlay = true},
                    .shadow = {.color = cod_rgba(0, 0, 0, 0.85f), .offset_y = 1.0f, .blur = 2.0f},
                    .transform = opt_some(HzUITransform, ((HzUITransform){.rotation = 45.0f})),
                    .pointer_capture_mode = COD_PASS}) {}
        ui_text(cod_ui_str(p->label.value), cod_text(u, COD_FONT_BODY, 9.5f * k, UI_FONT_BOLD, (HzUIColor){8, 22, 28, 255}, 0.96f));
      }
    }
    ui_element({.layout = {.margin = {.top = (u16)(2.4f * k + 0.5f)}}, .pointer_capture_mode = COD_PASS}) {
      ui_text(cod_ui_str(dist.value), cod_sh(cod_text(u, COD_FONT_BODY, 10.0f * k, UI_FONT_SEMIBOLD, COD_INK, 0.12f * 10.0f * k), k));
    }
    ui_text(cod_ui_str(p->name.value), cod_sh(cod_text(u, COD_FONT_BODY, 9.0f * k, UI_FONT_SEMIBOLD, COD_INK2, 0.18f * 9.0f * k), k));
  }
}

hz_internal hz_noinline void cod_hud_draw_nade(const CodUi *u, const CodHudNadeView *n, f32 k) {
  f32 remain = 1.0f - n->age / (n->life > 0.0f ? n->life : 1.0f);
  f32 rate = 2.2f + (1.0f - remain) * 5.0f;
  f32 ph = n->age * rate - m_floorf(n->age * rate);
  f32 rs = 0.7f + 0.9f * ease_cubic_out(ph);
  f32 ring = 30.0f * k;
  u16 stroke = (u16)m_maxf(1.0f, m_floorf(1.5f * k));
  ui_element({.floating = cod_float_abs(UI_ATTACH_POINT_CENTER_CENTER, n->screen, COD_Z_WORLD),
              .opacity = opt_some(f32, m_saturatef(remain * 4.0f)),
              .pointer_capture_mode = COD_PASS}) {
    ui_element({.layout = {.sizing = cod_fixed2(ring, ring)},
                .floating = cod_float_parent(UI_ATTACH_POINT_CENTER_CENTER, UI_ATTACH_POINT_CENTER_CENTER, (v2){0, 0}),
                .corner_radius = ui_corner_radius(ring * 0.5f),
                .border = {.color = COD_RED, .width = {stroke, stroke, stroke, stroke, 0}, .overlay = true},
                .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {rs, rs}})),
                .opacity = opt_some(f32, 0.9f * (1.0f - ph)),
                .pointer_capture_mode = COD_PASS}) {}
    f32 s = 15.0f / 16.0f * k;
    ui_element({.layout = {.sizing = cod_fixed2(10.8f * s, 10.8f * s)},
                .floating = cod_float_parent(UI_ATTACH_POINT_CENTER_CENTER, UI_ATTACH_POINT_CENTER_CENTER, (v2){0, 0}),
                .background_color = cod_rgba(255, 63, 49, 0.95f),
                .corner_radius = ui_corner_radius(5.4f * s),
                .border = {.color = cod_rgba(0, 0, 0, 0.5f), .width = {1, 1, 1, 1, 0}, .overlay = true},
                .shadow = {.color = cod_rgba(0, 0, 0, 0.9f), .offset_y = 1.0f, .blur = 2.0f},
                .pointer_capture_mode = COD_PASS}) {}
    ui_element({.layout = {.sizing = cod_fixed2(1.6f * s, 3.2f * s)},
                .floating = cod_float_parent(UI_ATTACH_POINT_CENTER_CENTER, UI_ATTACH_POINT_CENTER_CENTER, (v2){0, -6.8f * s}),
                .background_color = cod_rgba(255, 63, 49, 0.95f),
                .pointer_capture_mode = COD_PASS}) {}
    ui_element({.floating = cod_float_parent(UI_ATTACH_POINT_CENTER_TOP, UI_ATTACH_POINT_CENTER_CENTER, (v2){0, 13.0f * k}),
                .pointer_capture_mode = COD_PASS}) {
      ui_text(n->distance < 9.0f ? ui_string("DANGER CLOSE") : ui_string("GRENADE"),
              cod_sh(cod_text(u, COD_FONT_BODY, 9.0f * k, UI_FONT_SEMIBOLD, COD_RED, 0.24f * 9.0f * k), k));
    }
  }
}

hz_internal hz_noinline void cod_hud_draw_number(const CodUi *u, const CodHudNumberView *n, f32 k) {
  if (!n->in_front)
    return;
  f32 life = n->life > 0.0f ? n->life : 1.0f;
  f32 t = n->age / life;
  if (t >= 1.0f)
    return;
  f32 rise = ease_cubic_out(m_saturatef(t * 1.15f)) * 42.0f * k * n->rise_scale;
  f32 drift = n->drift * k * ease_quad_out(t);
  f32 pop = 1.0f + 0.35f * (1.0f - ease_quint_out(m_saturatef(t / 0.12f)));
  f32 a = (t < 0.55f ? 1.0f : 1.0f - ease_quad_in((t - 0.55f) / 0.45f)) * m_saturatef(2.6f - n->distance / 90.0f);
  HzUIColor color = COD_INK;
  f32 size = 17.0f;
  if (n->kind == COD_NUM_HS) {
    color = COD_AMBER;
    size = 21.0f;
  } else if (n->kind == COD_NUM_KILL) {
    color = COD_RED;
    size = 23.0f;
  } else if (n->kind == COD_NUM_ARMOUR) {
    color = COD_CYAN;
  }
  HzUITextConfig c = cod_text(u, COD_FONT_DISPLAY, size * k, UI_FONT_BOLD, color, 0.03f * size * k);
  c.shadows[0] = (HzUITextShadow){.color = cod_rgba(0, 0, 0, 0.95f), .offset_y = 1.0f, .blur = 2.0f};
  c.shadows[1] = (HzUITextShadow){.color = cod_rgba(0, 0, 0, 0.6f), .blur = 8.0f * k};
  ui_element({.floating = cod_float_abs(UI_ATTACH_POINT_CENTER_CENTER, (v2){n->screen.x + drift, n->screen.y - rise}, COD_Z_WORLD),
              .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {pop, pop}})),
              .opacity = opt_some(f32, a),
              .pointer_capture_mode = COD_PASS}) {
    ui_text(cod_ui_str(n->text.value), c);
  }
}
