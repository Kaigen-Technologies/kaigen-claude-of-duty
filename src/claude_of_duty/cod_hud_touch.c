// Claude of Duty touch controls: the touch_controls block's geometry in the HUD's ink, kept faint so the street reads through.

#define COD_TOUCH_FILL ((HzUIColor){5, 9, 12, 70})

hz_internal void cod_touch_disc(HzUIElementId id, v2 c, f32 d, HzUIColor fill, HzUIColor edge) {
  ui_element({.id = id,
              .floating = cod_float_abs(UI_ATTACH_POINT_LEFT_TOP, (v2){c.x - d * 0.5f, c.y - d * 0.5f}, COD_Z_CHROME),
              .layout = {.sizing = cod_fixed2(d, d)},
              .background_color = fill,
              .border = {.color = edge, .width = ui_border_outside(1)},
              .corner_radius = ui_corner_radius(d * 0.5f),
              .pointer_capture_mode = COD_PASS}) {}
}

// alert: rim and label in amber while the fill stays faint (a cue, not a press)
hz_internal hz_noinline void cod_touch_button(const CodUi *u, const TouchControls *tc, u32 b, b32 primary, b32 alert) {
  TouchButtonView bv = touch_controls_button(tc, b);
  if (!bv.visible)
    return;
  b32 on = bv.held || bv.lit;
  // pressed: the disc sinks a little and fills with amber
  f32 d = bv.diameter_px * (bv.held ? 0.94f : 1.0f);
  HzUIColor fill = on ? cod_alpha(COD_AMBER, bv.held ? 0.3f : 0.18f) : COD_TOUCH_FILL;
  HzUIColor edge = on || alert ? cod_alpha(COD_AMBER, 0.85f) : cod_alpha(COD_WHITE, primary ? 0.45f : 0.28f);
  HzUIColor ink = on || alert ? COD_AMBER : COD_INK;
  f32 size = primary ? 12.0f : 9.5f;
  ui_element({.id = ui_idi("CodTouchButton", b),
              .floating = cod_float_abs(UI_ATTACH_POINT_LEFT_TOP, (v2){bv.center_px.x - d * 0.5f, bv.center_px.y - d * 0.5f}, COD_Z_CHROME),
              .layout = {.sizing = cod_fixed2(d, d), .child_alignment = {UI_ALIGN_X_CENTER, UI_ALIGN_Y_CENTER}},
              .background_color = fill,
              .border = {.color = edge, .width = ui_border_outside(1)},
              .corner_radius = ui_corner_radius(d * 0.5f),
              .backdrop_blur = 2.0f,
              .pointer_capture_mode = COD_PASS}) {
    ui_text(cod_ui_str(bv.label), cod_sh_hard(cod_text(u, COD_FONT_BODY, size, UI_FONT_BOLD, ink, 0.16f * size)));
  }
  // fire carries the crosshair's ring
  if (primary)
    cod_touch_disc(ui_idi("CodTouchSight", b), bv.center_px, d * 0.62f, COD_CLEAR, cod_alpha(on ? COD_AMBER : COD_WHITE, on ? 0.5f : 0.18f));
}

hz_internal hz_noinline void cod_hud_draw_touch(const CodUi *u, const CodTouch *t, const CodHudModel *m) {
  const TouchControls *tc = &t->tc;
  TouchStickView s = touch_controls_stick(tc);
  if (s.visible) {
    HzUIColor thumb = t->sprint ? COD_AMBER : COD_WHITE;
    cod_touch_disc(ui_id("CodTouchStickBase"), s.center_px, s.outer_radius_px * 2.0f, COD_TOUCH_FILL, cod_alpha(COD_WHITE, 0.28f));
    cod_touch_disc(ui_id("CodTouchStickReach"), s.center_px, (s.max_speed_radius_px + s.thumb_radius_px) * 2.0f, COD_CLEAR,
                   cod_alpha(COD_WHITE, 0.12f));
    cod_touch_disc(ui_id("CodTouchStickThumb"), s.thumb_px, s.thumb_radius_px * 2.0f, cod_alpha(thumb, 0.3f), cod_alpha(thumb, 0.85f));
  }
  // an empty magazine lights reload, as the ammo panel's cue does
  b32 needs_reload = m->ammo == 0 && !m->reloading;
  cod_touch_button(u, tc, COD_TOUCH_FIRE, true, false);
  cod_touch_button(u, tc, COD_TOUCH_ADS, false, false);
  cod_touch_button(u, tc, COD_TOUCH_JUMP, false, false);
  cod_touch_button(u, tc, COD_TOUCH_CROUCH, false, false);
  cod_touch_button(u, tc, COD_TOUCH_RELOAD, false, needs_reload);
  cod_touch_button(u, tc, COD_TOUCH_SWAP, false, false);
  cod_touch_button(u, tc, COD_TOUCH_PAUSE, false, false);
}
