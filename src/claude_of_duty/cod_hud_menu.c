// Claude of Duty pause menu: the settings rows write CodMenuRequests, the flow applies them through CodSettings.

#define COD_MENU_HOVERS 8
#define COD_MENU_SENS_MIN 0.2f
#define COD_MENU_SENS_MAX 3.0f
#define COD_MENU_FOV_MIN 65.0f
#define COD_MENU_FOV_MAX 120.0f

#define COD_HAIR2 ((HzUIColor){255, 255, 255, 17.85f})

typedef struct CodMenuFrame {
  CodUi *u;
  InputSystem *input;
  const CodSettings *cur;
  CodMenuRequests *req;
  f32 k;
  f32 raw_dt;
  b32 hand;  // a control is hovered: the pointer becomes a hand
  b32 touch; // the keys line means nothing on a touch screen
} CodMenuFrame;

// the settings a request starts from: the pending ones when an edit is already queued this frame
hz_internal CodSettings *cod_menu_edit(CodMenuFrame *f) {
  if (!(f->req->queue & COD_MENU_SETTINGS))
    f->req->settings = *f->cur;
  f->req->queue |= COD_MENU_SETTINGS;
  return &f->req->settings;
}

hz_internal f32 cod_menu_hover(CodMenuFrame *f, u32 slot, b32 hovered) {
  return ui_tween(&f->u->menu.hover[slot], hovered ? 1.0f : 0.0f, 0.12f, ui_easing_ease(), f->raw_dt);
}

// one segmented-control button; true when released over it
hz_internal hz_noinline b32 cod_menu_seg_button(CodMenuFrame *f, HzUIElementId id, u32 slot, const char *label, b32 on, b32 last) {
  f32 k = f->k;
  CodHudMenu *menu = &f->u->menu;
  f32 h = cod_menu_hover(f, slot, menu->hover_on[slot]);
  HzUIColor bg = on ? COD_INK : ui_color_lerp((HzUIColor){255, 255, 255, 7.65f}, (HzUIColor){255, 255, 255, 17.85f}, h);
  HzUIColor fg = on ? (HzUIColor){11, 13, 15, 255} : ui_color_lerp(COD_INK2, COD_INK, h);
  u16 edge = 1;
  b32 clicked = false;
  ui_element({.id = id,
              .layout = {.padding = {(u16)(8.8f * k + 0.5f), (u16)(8.8f * k + 0.5f), (u16)(5.2f * k + 0.5f), (u16)(5.2f * k + 0.5f)}},
              .background_color = bg,
              .border = {.color = COD_HAIR, .width = {edge, last ? edge : 0, edge, edge, 0}}}) {
    b32 hov = ui_hovered();
    menu->hover_on[slot] = hov;
    if (hov) {
      f->hand = true;
      clicked = f->input->buttons[MOUSE_LEFT].released_this_frame;
    }
    ui_text(cod_ui_str(label), cod_text(f->u, COD_FONT_BODY, 10.0f * k, UI_FONT_SEMIBOLD, fg, 0.16f * 10.0f * k));
  }
  return clicked;
}

// a button of the bottom row
hz_internal hz_noinline b32 cod_menu_button(CodMenuFrame *f, HzUIElementId id, u32 slot, const char *label, b32 primary) {
  f32 k = f->k;
  CodHudMenu *menu = &f->u->menu;
  f32 h = cod_menu_hover(f, slot, menu->hover_on[slot]);
  HzUIColor bg = primary ? ui_color_lerp(COD_AMBER, (HzUIColor){255, 194, 81, 255}, h)
                         : ui_color_lerp((HzUIColor){255, 255, 255, 10.2f}, (HzUIColor){255, 255, 255, 25.5f}, h);
  HzUIColor edge = primary ? bg : ui_color_lerp(COD_HAIR, (HzUIColor){255, 255, 255, 102.0f}, h);
  HzUIColor fg = primary ? (HzUIColor){16, 11, 2, 255} : COD_INK;
  b32 clicked = false;
  ui_element({.id = id,
              .layout = {.padding = {(u16)(20.0f * k + 0.5f), (u16)(20.0f * k + 0.5f), (u16)(8.8f * k + 0.5f), (u16)(8.8f * k + 0.5f)}},
              .background_color = bg,
              .border = {.color = edge, .width = {1, 1, 1, 1, 0}}}) {
    b32 hov = ui_hovered();
    menu->hover_on[slot] = hov;
    if (hov) {
      f->hand = true;
      clicked = f->input->buttons[MOUSE_LEFT].released_this_frame;
    }
    ui_text(cod_ui_str(label), cod_text(f->u, COD_FONT_BODY, 11.0f * k, UI_FONT_SEMIBOLD, fg, 0.2f * 11.0f * k));
  }
  return clicked;
}

// the row's frame: name on the left, the control on the right, a hairline under it
#define cod_menu_row(k_)                                                                                                         \
  ui_element({.layout = {.sizing = {.width = ui_sizing_grow(0)},                                                                  \
                         .child_gap = (u16)(16.0f * (k_) + 0.5f),                                                                  \
                         .padding = {0, 0, (u16)(12.8f * (k_) + 0.5f), (u16)(12.8f * (k_) + 0.5f)},                                \
                         .child_alignment = {.y = UI_ALIGN_Y_CENTER}},                                                           \
              .border = {.color = COD_HAIR2, .width = {.bottom = 1}}})

hz_internal void cod_menu_row_name(const CodMenuFrame *f, const char *name) {
  f32 k = f->k;
  ui_element({.layout = {.sizing = {.width = ui_sizing_grow(0)}}}) {
    ui_text(cod_ui_str(name), cod_text(f->u, COD_FONT_BODY, 11.5f * k, UI_FONT_SEMIBOLD, COD_INK, 0.2f * 11.5f * k));
  }
}

// an absolute range slider: a press on the track jumps to the point and a drag follows, like a native range input
hz_internal hz_noinline b32 cod_menu_slider(CodMenuFrame *f, HzUIElementId id, u32 index, f32 min, f32 max, f32 step, f32 *value) {
  f32 k = f->k;
  CodHudMenu *menu = &f->u->menu;
  f32 w = 190.0f * k, h = 18.0f * k;
  b32 changed = false;
  HzUIElementData data = ui_get_element_data(id);
  const AppButton *mb = &f->input->buttons[MOUSE_LEFT];
  vec2 m = f->input->mouse_pos;
  if (data.found) {
    Clay_BoundingBox b = data.boundingBox;
    if (mb->pressed_this_frame && m.x >= b.x && m.x <= b.x + b.width && m.y >= b.y && m.y <= b.y + b.height)
      menu->dragging = (i32)index + 1;
    if (menu->dragging == (i32)index + 1) {
      if (mb->is_pressed && b.width > 0.0f) {
        f32 t = m_saturatef((m.x - b.x) / b.width);
        f32 v = min + t * (max - min);
        v = m_roundf(v / step) * step;
        v = m_clampf(v, min, max);
        if (v != *value) {
          *value = v;
          changed = true;
        }
      } else if (!mb->is_pressed) {
        menu->dragging = 0;
      }
    }
  }
  f32 t = m_saturatef((*value - min) / (max - min));
  ui_element({.id = id, .layout = {.sizing = cod_fixed2(w, h), .layout_direction = UI_STACK, .child_alignment = {.y = UI_ALIGN_Y_CENTER}}}) {
    if (ui_hovered() || menu->dragging == (i32)index + 1)
      f->hand = true;
    ui_element({.layout = {.sizing = {ui_sizing_grow(0), ui_sizing_fixed(2.0f * k)}}, .background_color = cod_rgba(255, 255, 255, 0.16f)}) {}
    ui_element({.layout = {.sizing = {ui_sizing_percent(t > 0.0005f ? t : 0.0005f), ui_sizing_fixed(2.0f * k)}}, .background_color = COD_AMBER}) {}
    f32 kn = 9.0f * k;
    ui_element({.layout = {.sizing = cod_fixed2(kn, kn)},
                .floating = cod_float_parent(UI_ATTACH_POINT_CENTER_CENTER, UI_ATTACH_POINT_LEFT_CENTER, (v2){t * w, 0}),
                .background_color = COD_AMBER,
                .shadow = {.color = cod_rgba(255, 176, 42, 0.5f), .blur = 6.0f * k},
                .transform = opt_some(HzUITransform, ((HzUITransform){.rotation = 45.0f})),
                .pointer_capture_mode = COD_PASS}) {}
  }
  return changed;
}

hz_internal void cod_menu_value(const CodMenuFrame *f, const char *text) {
  f32 k = f->k;
  ui_element({.layout = {.sizing = {.width = ui_sizing_fixed(46.0f * k)}, .child_alignment = {.x = UI_ALIGN_X_RIGHT}}}) {
    ui_text(cod_ui_str(text), cod_text(f->u, COD_FONT_MONO, 11.0f * k, UI_FONT_SEMIBOLD, COD_AMBER, 0.04f * 11.0f * k));
  }
}

hz_internal const char *cod_menu_preset_name(u32 i) {
  switch (i) {
  case 0: return "LOW";
  case 1: return "MEDIUM";
  case 2: return "HIGH";
  default: return "ULTRA";
  }
}

hz_internal hz_noinline void cod_menu_rows(CodMenuFrame *f) {
  f32 k = f->k;
  const CodSettings *cur = f->cur;
  cod_menu_row(k) {
    cod_menu_row_name(f, "GRAPHICS PRESET");
    ui_element({}) {
      for (u32 i = 0; i < 4; i++)
        if (cod_menu_seg_button(f, ui_idi("CodMenuQuality", i), i, cod_menu_preset_name(i), cur->quality == (i32)i, i == 3))
          cod_menu_edit(f)->quality = (i32)i;
    }
  }

  cod_menu_row(k) {
    cod_menu_row_name(f, f->touch ? "LOOK SENSITIVITY" : "MOUSE SENSITIVITY");
    f32 v = cur->sensitivity;
    if (cod_menu_slider(f, ui_id("CodMenuSens"), 0, COD_MENU_SENS_MIN, COD_MENU_SENS_MAX, 0.01f, &v))
      cod_menu_edit(f)->sensitivity = v;
    String32Bytes s = {0};
    cstr_format(s.value, sizeof(s.value), "%", fmt_f32_p(f->req->queue & COD_MENU_SETTINGS ? f->req->settings.sensitivity : v, 2));
    cod_menu_value(f, s.value);
  }

  cod_menu_row(k) {
    cod_menu_row_name(f, "FIELD OF VIEW");
    f32 v = cur->fov;
    if (cod_menu_slider(f, ui_id("CodMenuFov"), 1, COD_MENU_FOV_MIN, COD_MENU_FOV_MAX, 1.0f, &v))
      cod_menu_edit(f)->fov = v;
    String32Bytes s = {0};
    cstr_format(s.value, sizeof(s.value), "%", fmt_u32((u32)(f->req->queue & COD_MENU_SETTINGS ? f->req->settings.fov : v)));
    cod_menu_value(f, s.value);
  }

  cod_menu_row(k) {
    cod_menu_row_name(f, "VOLUME");
    f32 v = cur->volume;
    if (cod_menu_slider(f, ui_id("CodMenuVolume"), 2, 0.0f, 1.0f, 0.01f, &v))
      cod_menu_edit(f)->volume = v;
    String32Bytes s = {0};
    cstr_format(s.value, sizeof(s.value), "%", fmt_u32((u32)m_roundf((f->req->queue & COD_MENU_SETTINGS ? f->req->settings.volume : v) * 100.0f)));
    cod_menu_value(f, s.value);
  }

  cod_menu_row(k) {
    cod_menu_row_name(f, "INVERT LOOK");
    ui_element({}) {
      if (cod_menu_seg_button(f, ui_idi("CodMenuInvert", 0), 4, "OFF", !cur->invert_y, false))
        cod_menu_edit(f)->invert_y = false;
      if (cod_menu_seg_button(f, ui_idi("CodMenuInvert", 1), 5, "ON", cur->invert_y, true))
        cod_menu_edit(f)->invert_y = true;
    }
  }
}

hz_internal hz_noinline void cod_menu_panel(CodMenuFrame *f) {
  f32 k = f->k;
  ui_element({.layout = {.sizing = {.width = ui_sizing_fixed(430.0f * k)},
                         .layout_direction = UI_TOP_TO_BOTTOM,
                         .padding = {(u16)(18.0f * k + 0.5f), 0, 0, 0}},
              .border = {.color = COD_AMBER, .width = {.left = (u16)m_maxf(1.0f, m_roundf(2.0f * k))}}}) {
    HzUITextConfig title = cod_text(f->u, COD_FONT_DISPLAY, 46.0f * k, UI_FONT_BOLD, COD_INK, 0.3f * 46.0f * k);
    title.shadows[0] = (HzUITextShadow){.color = cod_rgba(0, 0, 0, 0.8f), .offset_y = 2.0f, .blur = 6.0f};
    ui_text(ui_string("PAUSED"), title);
    ui_element({.layout = {.margin = {.top = (u16)(4.8f * k + 0.5f)}}}) {
      ui_text(ui_string("OVERWATCH \xE2\x80\x94 TACTICAL OPERATIONS"),
              cod_text(f->u, COD_FONT_BODY, 10.0f * k, UI_FONT_SEMIBOLD, COD_INK3, 0.28f * 10.0f * k));
    }
    ui_element({.layout = {.sizing = {ui_sizing_grow(0), ui_sizing_fixed(1.0f)}, .margin = {.top = (u16)(20.0f * k + 0.5f), .bottom = (u16)(8.0f * k + 0.5f)}},
                .gradient = {.kind = UI_GRADIENT_LINEAR,
                             .angle = 90.0f,
                             .stop_count = 2,
                             .stops = {{0.0f, {255, 255, 255, 71.4f}}, {1.0f, {255, 255, 255, 0}}}}}) {}
    cod_menu_rows(f);
    ui_element({.layout = {.child_gap = (u16)(10.0f * k + 0.5f), .margin = {.top = (u16)(20.0f * k + 0.5f)}}}) {
      if (cod_menu_button(f, ui_id("CodMenuResume"), 6, "RESUME", true))
        f->req->queue |= COD_MENU_RESUME;
      if (cod_menu_button(f, ui_id("CodMenuDefaults"), 7, "DEFAULTS", false)) {
        CodSettings *s = cod_menu_edit(f);
        s->sensitivity = 1.0f;
        s->fov = 80.0f;
        s->invert_y = false;
        s->quality = cod_quality_default();
        s->volume = 1.0f;
      }
    }
    if (!f->touch) {
      ui_element({.layout = {.margin = {.top = (u16)(16.0f * k + 0.5f)}}}) {
        ui_text(ui_string("ESC RESUME \xC2\xB7 WASD MOVE \xC2\xB7 SHIFT SPRINT \xC2\xB7 R RELOAD \xC2\xB7 F USE"),
                cod_text(f->u, COD_FONT_BODY, 9.5f * k, UI_FONT_SEMIBOLD, COD_INK3, 0.2f * 9.5f * k));
      }
    }
  }
}

// the whole-screen layer; shown is the eased fade (0 hides it)
hz_internal hz_noinline void cod_hud_draw_menu(CodMenuFrame *f, const CodHudView *v, f32 shown) {
  f32 k = f->k;
  ui_element({.layout = {.sizing = cod_fixed2(v->w, v->h), .child_alignment = {.y = UI_ALIGN_Y_CENTER}, .padding = {(u16)(88.0f * k + v->safe.left + 0.5f), 0, 0, 0}},
              .floating = cod_float_root(UI_ATTACH_POINT_LEFT_TOP, (v2){0, 0}, COD_Z_MENU),
              .background_color = cod_rgba(0, 0, 0, 0.2f),
              .backgrounds = ui_backgrounds({.gradient = {.kind = UI_GRADIENT_LINEAR,
                                                          .angle = 105.0f,
                                                          .stop_count = 3,
                                                          .stops = {{0.0f, {4, 6, 8, 229.5f}}, {0.46f, {4, 6, 8, 183.6f}}, {1.0f, {4, 6, 8, 107.1f}}}}}),
              .backdrop_blur = 9.0f * k,
              .backdrop_saturate = opt_some(f32, 0.7f),
              .opacity = opt_some(f32, shown),
              .pointer_capture_mode = (HzUIPointerCaptureMode)0}) {
    cod_menu_panel(f);
  }
}
