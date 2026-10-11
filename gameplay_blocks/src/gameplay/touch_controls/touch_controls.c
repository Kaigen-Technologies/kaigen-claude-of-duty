#include "touch_controls.h"
#include "lib/assert.h"
#include "os/os.h"
#include "ui/ui.h"

// layout defaults, all logical px; overridable per orientation via
// TouchControlsConfig.

// DELIBERATE DIVERGENCE from the landscape stick rect: its right edge sat
// where a centred pinch's left finger lands, so the pinch rotated instead of
// zooming. Anchored bottom-left and shrunk.
#define TC_DT_LANDSCAPE_X_PCT 0.0f
#define TC_DT_LANDSCAPE_Y_PCT 0.50f
#define TC_DT_LANDSCAPE_W_PCT 0.35f
#define TC_DT_LANDSCAPE_H_PCT 0.50f

// DELIBERATE DIVERGENCE from the portrait rect (full-width, lower 40%):
// it swallowed every lower-screen pinch and the jump button. Halved to the
// bottom-LEFT quadrant.
#define TC_DT_PORTRAIT_X_PCT 0.0f
#define TC_DT_PORTRAIT_Y_PCT 0.55f
#define TC_DT_PORTRAIT_W_PCT 0.50f
#define TC_DT_PORTRAIT_H_PCT 0.45f

#define TC_STICK_OUTER_RADIUS_PX 45.0f
#define TC_STICK_RING_RADIUS_PX 20.0f
#define TC_STICK_MIDDLE_RADIUS_PX 10.0f

#define TC_STICK_DEADZONE_RADIUS_PX 2.0f
#define TC_STICK_MAX_SPEED_RADIUS_PX 20.0f
// response curve: 0 below the deadzone, else unit*min(1, mag/max_speed_radius)

// diameter 70px and a 120px corner offset to the button CENTER: our reading
// of an ambiguous source cell, not a verified transcription.
#define TC_JUMP_BUTTON_DIAMETER_PX 70.0f
#define TC_JUMP_BUTTON_CORNER_OFFSET_PX 120.0f

#define TC_SMALL_SCREEN_MAX_DIM_PX 500.0f
#define TC_SMALL_SCREEN_UI_SCALE 0.8f

// pinch rescue window; no equivalent. 150ms catches a deliberate
// two-finger placement while the walk it cancels is still a nudge.
#define TC_PINCH_GRACE_S 0.15f

hz_internal b32 tc_point_in_rect(v2 p, v2 pos, v2 size) {
  return p.x >= pos.x && p.x <= pos.x + size.x && p.y >= pos.y &&
         p.y <= pos.y + size.y;
}

hz_internal b32 tc_point_in_circle(v2 p, v2 center, f32 radius) {
  return v2_lengthsq(v2_sub(p, center)) <= radius * radius;
}

// the touch is held by an action button
hz_internal b32 tc_is_button_touch(const TouchControls *tc, u32 id) {
  for (u32 b = 0; b < TOUCH_BUTTON_MAX; b++) {
    if (tc->button_claimed[b] && tc->button_touch_id[b] == id)
      return true;
  }
  return false;
}

hz_internal v2 tc_anchor_point(TouchAnchor anchor, v2 offset, f32 x0, f32 y0, f32 w, f32 h) {
  switch (anchor) {
  case TOUCH_ANCHOR_BOTTOM_LEFT: return (v2){x0 + offset.x, y0 + h - offset.y};
  case TOUCH_ANCHOR_TOP_RIGHT: return (v2){x0 + w - offset.x, y0 + offset.y};
  case TOUCH_ANCHOR_TOP_LEFT: return (v2){x0 + offset.x, y0 + offset.y};
  default: return (v2){x0 + w - offset.x, y0 + h - offset.y};
  }
}

hz_internal const MobileTouch *tc_find_live_touch(InputSystem *in, u32 id) {
  for (i32 i = 0; i < MAX_TOUCHES; i++) {
    const MobileTouch *t = &in->touches.items[i];
    if ((t->is_active || t->stopped_this_frame) && t->id == id)
      return t;
  }
  return NULL;
}

TouchControlsLayout touch_controls_layout_default(TouchOrientation orientation) {
  TouchControlsLayout l = {
      .custom = true,
      .stick_outer_radius_px = TC_STICK_OUTER_RADIUS_PX,
      .stick_ring_radius_px = TC_STICK_RING_RADIUS_PX,
      .stick_middle_radius_px = TC_STICK_MIDDLE_RADIUS_PX,
      .deadzone_radius_px = TC_STICK_DEADZONE_RADIUS_PX,
      .max_speed_radius_px = TC_STICK_MAX_SPEED_RADIUS_PX,
      .jump_diameter_px = TC_JUMP_BUTTON_DIAMETER_PX,
      .jump_corner_offset_px = {TC_JUMP_BUTTON_CORNER_OFFSET_PX,
                                TC_JUMP_BUTTON_CORNER_OFFSET_PX},
      .pinch_grace_s = TC_PINCH_GRACE_S,
  };
  if (orientation == TOUCH_ORIENTATION_PORTRAIT) {
    l.region_pos_pct = (v2){TC_DT_PORTRAIT_X_PCT, TC_DT_PORTRAIT_Y_PCT};
    l.region_size_pct = (v2){TC_DT_PORTRAIT_W_PCT, TC_DT_PORTRAIT_H_PCT};
  } else {
    l.region_pos_pct = (v2){TC_DT_LANDSCAPE_X_PCT, TC_DT_LANDSCAPE_Y_PCT};
    l.region_size_pct = (v2){TC_DT_LANDSCAPE_W_PCT, TC_DT_LANDSCAPE_H_PCT};
  }
  return l;
}

TouchControls touch_controls_init(TouchControlsConfig cfg) {
  TouchControls tc = {0};
  tc.config = cfg;
  debug_assert_msg(cfg.thumbstick_mode == TOUCH_THUMBSTICK_DYNAMIC,
                   "touch_controls: TOUCH_THUMBSTICK_FIXED is reserved and "
                   "unimplemented (mode %)",
                   fmt_u32((u32)cfg.thumbstick_mode));

  tc.resolved[TOUCH_ORIENTATION_LANDSCAPE] =
      cfg.landscape.custom ? cfg.landscape
                           : touch_controls_layout_default(TOUCH_ORIENTATION_LANDSCAPE);
  tc.resolved[TOUCH_ORIENTATION_PORTRAIT] =
      cfg.portrait.custom ? cfg.portrait
                          : touch_controls_layout_default(TOUCH_ORIENTATION_PORTRAIT);

  tc.small_screen_max_dim_px = cfg.small_screen_max_dim_px > 0.0f
                                   ? cfg.small_screen_max_dim_px
                                   : TC_SMALL_SCREEN_MAX_DIM_PX;
  tc.small_screen_ui_scale = cfg.small_screen_ui_scale > 0.0f
                                 ? cfg.small_screen_ui_scale
                                 : TC_SMALL_SCREEN_UI_SCALE;
  return tc;
}

TouchControlsFrame touch_controls_update(TouchControls *tc, InputSystem *in, f32 now) {
  TouchControlsFrame out = {0};
  out.active = in->last_category == INPUT_CATEGORY_TOUCH || tc->config.force_enabled;
  tc->last_active = out.active;

  ScreenDesc screen = os_get_screen_desc();
  OsSafeAreaInsets safe = screen.safe_area;

  f32 eff_x0 = safe.left;
  f32 eff_y0 = safe.top;
  f32 eff_w = (f32)screen.width - safe.left - safe.right;
  f32 eff_h = (f32)screen.height - safe.top - safe.bottom;
  if (eff_w < 0.0f)
    eff_w = 0.0f;
  if (eff_h < 0.0f)
    eff_h = 0.0f;

  f32 scale = tc->config.ui_scale;
  if (scale <= 0.0f) {
    f32 min_dim = (f32)screen.width < (f32)screen.height ? (f32)screen.width
                                                         : (f32)screen.height;
    scale = min_dim < tc->small_screen_max_dim_px ? tc->small_screen_ui_scale : 1.0f;
  }
  tc->resolved_ui_scale = scale;

  b32 landscape = screen.width >= screen.height;
  tc->last_landscape = landscape;
  const TouchControlsLayout *layout =
      &tc->resolved[landscape ? TOUCH_ORIENTATION_LANDSCAPE : TOUCH_ORIENTATION_PORTRAIT];

  v2 region_pos = {eff_x0 + layout->region_pos_pct.x * eff_w,
                   eff_y0 + layout->region_pos_pct.y * eff_h};
  v2 region_size = {layout->region_size_pct.x * eff_w,
                    layout->region_size_pct.y * eff_h};
  tc->stick_region_pos_px = region_pos;
  tc->stick_region_size_px = region_size;

  f32 jump_diam = layout->jump_diameter_px * scale;
  v2 jump_offset = v2_scale(layout->jump_corner_offset_px, scale);
  v2 jump_center = {eff_x0 + eff_w - jump_offset.x, eff_y0 + eff_h - jump_offset.y};
  tc->jump_button_center_px = jump_center;
  tc->jump_button_diameter_px = jump_diam;

  for (u32 b = 0; b < TOUCH_BUTTON_MAX; b++) {
    const TouchButtonPlacement *pl = &layout->buttons[b];
    b32 present = tc->config.buttons[b].enabled && pl->diameter_px > 0.0f;
    tc->button_diameter_px[b] = present ? pl->diameter_px * scale : 0.0f;
    tc->button_center_px[b] =
        tc_anchor_point(pl->anchor, v2_scale(pl->corner_offset_px, scale), eff_x0, eff_y0, eff_w, eff_h);
  }

  // a button's touch reports its end on the frame it stops, then the claim is dropped
  for (u32 b = 0; b < TOUCH_BUTTON_MAX; b++) {
    if (!tc->button_claimed[b])
      continue;
    const MobileTouch *t = tc_find_live_touch(in, tc->button_touch_id[b]);
    if (!t) {
      tc->button_claimed[b] = false;
      continue;
    }
    out.buttons[b].held = t->is_active;
    out.buttons[b].released = t->stopped_this_frame;
  }

  if (tc->stick_touch_claimed && !tc_find_live_touch(in, tc->stick_touch_id))
    tc->stick_touch_claimed = false;
  if (tc->jump_touch_claimed && !tc_find_live_touch(in, tc->jump_touch_id))
    tc->jump_touch_claimed = false;
  if (tc->stick_excluded_valid && !tc_find_live_touch(in, tc->stick_excluded_touch_id))
    tc->stick_excluded_valid = false;

  for (i32 i = 0; i < MAX_TOUCHES; i++) {
    const MobileTouch *t = &in->touches.items[i];
    if (!t->started_this_frame || t->claimed)
      continue;
    v2 start = {t->start_x, t->start_y};
    if (ui_point_over_ui(tc->config.ui, start))
      continue;

    b32 on_button = false;
    for (u32 b = 0; b < TOUCH_BUTTON_MAX && !on_button; b++) {
      if (tc->button_claimed[b] || tc->button_hidden[b] || tc->button_diameter_px[b] <= 0.0f ||
          !tc_point_in_circle(start, tc->button_center_px[b], tc->button_diameter_px[b] * 0.5f))
        continue;
      tc->button_claimed[b] = true;
      tc->button_touch_id[b] = t->id;
      tc->button_prev_px[b] = start;
      input_claim_touch(in, t->id);
      out.buttons[b].pressed = true;
      out.buttons[b].held = t->is_active;
      out.buttons[b].released = t->stopped_this_frame;
      on_button = true;
    }
    if (on_button)
      continue;

    if (!tc->config.disable_jump_button && !tc->jump_touch_claimed &&
        tc_point_in_circle(start, jump_center, jump_diam * 0.5f)) {
      tc->jump_touch_claimed = true;
      tc->jump_touch_id = t->id;
      input_claim_touch(in, t->id);
      continue;
    }
    if (tc->stick_excluded_valid && t->id == tc->stick_excluded_touch_id)
      continue;
    if (!tc->stick_touch_claimed && tc_point_in_rect(start, region_pos, region_size)) {
      tc->stick_touch_claimed = true;
      tc->stick_touch_id = t->id;
      tc->stick_center_px = start;
      input_claim_touch(in, t->id);
      continue;
    }
  }

  if (tc->stick_touch_claimed && layout->pinch_grace_s > 0.0f) {
    const MobileTouch *stick_touch = tc_find_live_touch(in, tc->stick_touch_id);
    if (stick_touch) {
      for (i32 i = 0; i < MAX_TOUCHES; i++) {
        const MobileTouch *t = &in->touches.items[i];
        if (!t->is_active && !t->stopped_this_frame)
          continue;
        if (t->id == tc->stick_touch_id)
          continue;
        if (tc->jump_touch_claimed && t->id == tc->jump_touch_id)
          continue;
        if (tc_is_button_touch(tc, t->id))
          continue;
        if (m_fabsf(stick_touch->start_time - t->start_time) >= layout->pinch_grace_s)
          continue;

        tc->stick_touch_claimed = false;
        tc->stick_excluded_valid = true;
        tc->stick_excluded_touch_id = tc->stick_touch_id;
        tc->stick_thumb_offset_px = (v2){0, 0};
        break;
      }
    }
  }

  if (tc->stick_touch_claimed) {
    const MobileTouch *t = tc_find_live_touch(in, tc->stick_touch_id);
    if (t) {
      v2 cur = {t->current_x, t->current_y};
      v2 offset = v2_sub(cur, tc->stick_center_px);
      f32 mag = v2_length(offset);
      f32 deadzone = layout->deadzone_radius_px * scale;
      f32 max_speed = layout->max_speed_radius_px * scale;
      out.stick_travel = max_speed > 0.0f ? mag / max_speed : 0.0f;
      if (mag < deadzone) {
        out.move = (v2){0, 0};
        tc->stick_thumb_offset_px = (v2){0, 0};
      } else {
        v2 unit = v2_scale(offset, 1.0f / mag);
        f32 speed = mag / max_speed;
        if (speed > 1.0f)
          speed = 1.0f;
        v2 move_screen = v2_scale(unit, speed);
        out.move = (v2){move_screen.x, -move_screen.y};
        f32 visual_mag = mag < max_speed ? mag : max_speed;
        tc->stick_thumb_offset_px = v2_scale(unit, visual_mag);
      }
    }
  }

  out.jump_held = tc->jump_touch_claimed;

  v2 look_through_px = {0, 0};
  for (u32 b = 0; b < TOUCH_BUTTON_MAX; b++) {
    if (!tc->button_claimed[b] || !tc->config.buttons[b].look_through)
      continue;
    const MobileTouch *t = tc_find_live_touch(in, tc->button_touch_id[b]);
    v2 cur = {t->current_x, t->current_y};
    look_through_px = v2_add(look_through_px, v2_sub(cur, tc->button_prev_px[b]));
    tc->button_prev_px[b] = cur;
    tc->last_pan_time = now;
  }

  u32 new_ids[2] = {0, 0};
  v2 new_pos[2] = {{0, 0}, {0, 0}};
  u32 new_count = 0;
  for (i32 i = 0; i < MAX_TOUCHES && new_count < 2; i++) {
    const MobileTouch *t = &in->touches.items[i];
    if (!t->is_active && !t->stopped_this_frame)
      continue;
    if (tc->stick_touch_claimed && t->id == tc->stick_touch_id)
      continue;
    if (tc->jump_touch_claimed && t->id == tc->jump_touch_id)
      continue;
    if (tc_is_button_touch(tc, t->id))
      continue;
    new_ids[new_count] = t->id;
    new_pos[new_count] = (v2){t->current_x, t->current_y};
    new_count++;
  }
  // no pinch: the earliest finger keeps the camera, so a second one never jumps the view
  if (tc->config.disable_pinch && new_count == 2) {
    u32 keep = tc_find_live_touch(in, new_ids[1])->start_time < tc_find_live_touch(in, new_ids[0])->start_time;
    if (tc->cam_touch_count == 1 && new_ids[1 - keep] == tc->cam_touch_ids[0])
      keep = 1 - keep;
    new_ids[0] = new_ids[keep];
    new_pos[0] = new_pos[keep];
    new_count = 1;
  }

  b32 same_participants = new_count == tc->cam_touch_count;
  if (same_participants) {
    for (u32 i = 0; i < new_count; i++) {
      if (new_ids[i] != tc->cam_touch_ids[i])
        same_participants = false;
    }
  }

  if (same_participants && new_count == 1) {
    out.camera_pan_px = v2_sub(new_pos[0], tc->cam_touch_prev_px[0]);
    tc->last_pan_time = now;
  } else if (same_participants && new_count == 2) {
    f32 prev_diam = v2_length(v2_sub(tc->cam_touch_prev_px[1], tc->cam_touch_prev_px[0]));
    f32 cur_diam = v2_length(v2_sub(new_pos[1], new_pos[0]));
    out.camera_pinch_px = cur_diam - prev_diam;
  } else if (new_count == 1) {
    tc->last_pan_time = now;
  }

  tc->cam_touch_count = new_count;
  for (u32 i = 0; i < new_count; i++) {
    tc->cam_touch_ids[i] = new_ids[i];
    tc->cam_touch_prev_px[i] = new_pos[i];
  }

  out.camera_pan_px = v2_add(out.camera_pan_px, look_through_px);
  out.last_pan_time = tc->last_pan_time;

  return out;
}

void touch_controls_set_button(TouchControls *tc, u32 index, b32 visible, b32 lit) {
  if (index >= TOUCH_BUTTON_MAX) {
    log_error("touch_controls_set_button: index % out of range", fmt_u32(index));
    return;
  }
  tc->button_hidden[index] = !visible;
  tc->button_lit[index] = lit;
}

TouchStickView touch_controls_stick(const TouchControls *tc) {
  const TouchControlsLayout *layout =
      &tc->resolved[tc->last_landscape ? TOUCH_ORIENTATION_LANDSCAPE : TOUCH_ORIENTATION_PORTRAIT];
  f32 scale = tc->resolved_ui_scale;
  return (TouchStickView){
      .visible = tc->last_active && tc->stick_touch_claimed,
      .center_px = tc->stick_center_px,
      .thumb_px = v2_add(tc->stick_center_px, tc->stick_thumb_offset_px),
      .outer_radius_px = layout->stick_outer_radius_px * scale,
      .ring_radius_px = layout->stick_ring_radius_px * scale,
      .thumb_radius_px = layout->stick_middle_radius_px * scale,
      .max_speed_radius_px = layout->max_speed_radius_px * scale,
      .region_pos_px = tc->stick_region_pos_px,
      .region_size_px = tc->stick_region_size_px,
  };
}

TouchButtonView touch_controls_button(const TouchControls *tc, u32 index) {
  if (index >= TOUCH_BUTTON_MAX) {
    log_error("touch_controls_button: index % out of range", fmt_u32(index));
    return (TouchButtonView){0};
  }
  f32 d = tc->button_diameter_px[index];
  return (TouchButtonView){
      .visible = tc->last_active && d > 0.0f && (!tc->button_hidden[index] || tc->button_claimed[index]),
      .held = tc->button_claimed[index],
      .lit = tc->button_lit[index],
      .center_px = tc->button_center_px[index],
      .diameter_px = d,
      .label = tc->config.buttons[index].label,
  };
}

TouchButtonView touch_controls_jump_button(const TouchControls *tc) {
  return (TouchButtonView){
      .visible = tc->last_active && !tc->config.disable_jump_button && tc->jump_button_diameter_px > 0.0f,
      .held = tc->jump_touch_claimed,
      .center_px = tc->jump_button_center_px,
      .diameter_px = tc->jump_button_diameter_px,
  };
}

hz_internal void tc_draw_circle(HzUIElementId id, v2 center, f32 diameter, HzUIColor color) {
  ui_element({
      .id = id,
      .floating = {.attach_to = UI_ATTACH_TO_ROOT,
                   .attach_points = {.element = UI_ATTACH_POINT_LEFT_TOP,
                                    .parent = UI_ATTACH_POINT_LEFT_TOP},
                   .offset = {center.x - diameter * 0.5f, center.y - diameter * 0.5f},
                   .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH},
      .layout = {.sizing = {.width = ui_sizing_fixed(diameter),
                           .height = ui_sizing_fixed(diameter)}},
      .background_color = color,
      .corner_radius = ui_corner_radius(diameter * 0.5f),
      .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH,
  }) {}
}

void touch_controls_draw_stick(const TouchControls *tc) {
  TouchStickView v = touch_controls_stick(tc);
  if (!v.visible)
    return;
  tc_draw_circle(ui_id("TouchStickOuter"), v.center_px, v.outer_radius_px * 2.0f, (HzUIColor){255, 255, 255, 60});
  tc_draw_circle(ui_id("TouchStickRing"), v.center_px, v.ring_radius_px * 2.0f, (HzUIColor){255, 255, 255, 90});
  tc_draw_circle(ui_id("TouchStickMiddle"), v.thumb_px, v.thumb_radius_px * 2.0f, (HzUIColor){255, 255, 255, 200});
}

void touch_controls_draw_button(const TouchControls *tc, u32 index) {
  TouchButtonView v = touch_controls_button(tc, index);
  if (!v.visible)
    return;
  HzUITextConfig label = tc->config.label;
  if (label.fontSize <= 0.0f) {
    label.fontSize = 14.0f * tc->resolved_ui_scale;
    label.textColor = (HzUIColor){255, 255, 255, 230};
  }
  label.textAlignment = CLAY_TEXT_ALIGN_CENTER;
  f32 d = v.diameter_px;
  ui_element({
      .id = ui_idi("TouchButton", index),
      .floating = {.attach_to = UI_ATTACH_TO_ROOT,
                   .attach_points = {.element = UI_ATTACH_POINT_LEFT_TOP, .parent = UI_ATTACH_POINT_LEFT_TOP},
                   .offset = {v.center_px.x - d * 0.5f, v.center_px.y - d * 0.5f},
                   .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH},
      .layout = {.sizing = {.width = ui_sizing_fixed(d), .height = ui_sizing_fixed(d)},
                 .child_alignment = {.x = UI_ALIGN_X_CENTER, .y = UI_ALIGN_Y_CENTER}},
      .background_color = v.held || v.lit ? (HzUIColor){255, 255, 255, 150} : (HzUIColor){255, 255, 255, 70},
      .corner_radius = ui_corner_radius(d * 0.5f),
      .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH,
  }) {
    if (v.label)
      ui_text(ui_str_c(v.label), label);
  }
}

void touch_controls_draw_jump_button(const TouchControls *tc) {
  TouchButtonView v = touch_controls_jump_button(tc);
  if (!v.visible)
    return;
  tc_draw_circle(ui_id("TouchJumpButton"), v.center_px, v.diameter_px,
                 v.held ? (HzUIColor){255, 255, 255, 160} : (HzUIColor){255, 255, 255, 90});
}

void touch_controls_draw(TouchControls *tc, UISystem *ui) {
  UNUSED(ui);
  touch_controls_draw_stick(tc);
  for (u32 b = 0; b < TOUCH_BUTTON_MAX; b++)
    touch_controls_draw_button(tc, b);
  touch_controls_draw_jump_button(tc);
}
