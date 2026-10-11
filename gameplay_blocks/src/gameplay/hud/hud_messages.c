#include "hud_messages.h"
#include "lib/assert.h"
#include "lib/thread_context.h"

// ---- state machine ----

hz_internal void hud_msg_start(HudMessages *h, u32 c, const HudMsg *m, b32 keep_level) {
  HudMsgChannelState *s = &h->channels[c];
  s->active = *m;
  s->serial++;
  s->hold_age = 0.0f;
  if (!keep_level)
    s->level = 0.0f;
  if (h->desc[c].enter_s > 0.0f) {
    s->phase = HUD_MSG_PHASE_ENTER;
  } else {
    s->level = 1.0f;
    s->phase = HUD_MSG_PHASE_HOLD;
  }
}

hz_internal void hud_msg_begin_exit(HudMessages *h, u32 c) {
  HudMsgChannelState *s = &h->channels[c];
  s->phase = HUD_MSG_PHASE_EXIT;
  if (h->desc[c].exit_s <= 0.0f) {
    s->level = 0.0f;
    s->phase = HUD_MSG_PHASE_IDLE;
  }
}

hz_internal void hud_msg_queue_insert(HudMsgChannelState *s, const HudMsg *m) {
  if (s->queue_len == HUD_MSG_QUEUE_MAX) {
    if (m->priority <= s->queue[HUD_MSG_QUEUE_MAX - 1].priority) {
      log_warn("hud_msg: channel queue full, message dropped");
      return;
    }
    s->queue_len--;
  }
  u32 at = s->queue_len;
  while (at > 0 && s->queue[at - 1].priority < m->priority) {
    s->queue[at] = s->queue[at - 1];
    at--;
  }
  s->queue[at] = *m;
  s->queue_len++;
}

hz_internal void hud_msg_submit(HudMessages *h, const HudMsg *m) {
  u32 c = (u32)m->channel;
  HudMsgChannelState *s = &h->channels[c];
  const HudMsgChannelDesc *d = &h->desc[c];
  if (m->dedupe != 0) {
    if (s->phase != HUD_MSG_PHASE_IDLE && s->active.dedupe == m->dedupe) {
      s->active = *m;
      s->hold_age = 0.0f;
      if (s->phase == HUD_MSG_PHASE_EXIT)
        s->phase = d->enter_s > 0.0f ? HUD_MSG_PHASE_ENTER : HUD_MSG_PHASE_HOLD;
      return;
    }
    for (u32 i = 0; i < s->queue_len; i++) {
      if (s->queue[i].dedupe == m->dedupe) {
        s->queue[i] = *m;
        return;
      }
    }
  }
  if (d->policy == HUD_MSG_POLICY_REPLACE) {
    s->queue_len = 0;
    if (s->phase == HUD_MSG_PHASE_ENTER || s->phase == HUD_MSG_PHASE_HOLD) {
      if (d->restart_on_replace) {
        hud_msg_start(h, c, m, false);
      } else {
        s->active = *m;
        s->hold_age = 0.0f;
      }
    } else {
      hud_msg_start(h, c, m, s->phase == HUD_MSG_PHASE_EXIT && !d->restart_on_replace);
    }
    return;
  }
  if (s->phase == HUD_MSG_PHASE_IDLE) {
    hud_msg_start(h, c, m, false);
    return;
  }
  hud_msg_queue_insert(s, m);
  if (m->priority > s->active.priority && s->phase != HUD_MSG_PHASE_EXIT)
    hud_msg_begin_exit(h, c);
}

hz_internal void hud_msg_clear_channel(HudMessages *h, u32 c) {
  HudMsgChannelState *s = &h->channels[c];
  s->queue_len = 0;
  s->pending_len = 0;
  if (s->phase == HUD_MSG_PHASE_ENTER || s->phase == HUD_MSG_PHASE_HOLD)
    hud_msg_begin_exit(h, c);
}

hz_internal void hud_msg_tick_channel(HudMessages *h, u32 c, f32 dt) {
  HudMsgChannelState *s = &h->channels[c];
  const HudMsgChannelDesc *d = &h->desc[c];
  for (u32 i = 0; i < s->pending_len;) {
    s->pending[i].wait -= dt;
    if (s->pending[i].wait > 0.0f) {
      i++;
      continue;
    }
    HudMsg m = s->pending[i].msg;
    for (u32 k = i + 1; k < s->pending_len; k++)
      s->pending[k - 1] = s->pending[k];
    s->pending_len--;
    hud_msg_submit(h, &m);
  }
  switch (s->phase) {
  case HUD_MSG_PHASE_ENTER:
    s->level = m_minf(1.0f, s->level + dt / d->enter_s);
    if (s->level >= 1.0f) {
      s->phase = HUD_MSG_PHASE_HOLD;
      s->hold_age = 0.0f;
    }
    break;
  case HUD_MSG_PHASE_HOLD:
    if (s->active.duration > 0.0f) {
      s->hold_age += dt;
      if (s->hold_age >= s->active.duration)
        hud_msg_begin_exit(h, c);
    }
    break;
  case HUD_MSG_PHASE_EXIT:
    s->level = m_maxf(0.0f, s->level - dt / d->exit_s);
    if (s->level <= 0.0f)
      s->phase = HUD_MSG_PHASE_IDLE;
    break;
  default: break;
  }
  if (s->phase == HUD_MSG_PHASE_IDLE && s->queue_len > 0) {
    HudMsg next = s->queue[0];
    for (u32 k = 1; k < s->queue_len; k++)
      s->queue[k - 1] = s->queue[k];
    s->queue_len--;
    hud_msg_start(h, c, &next, false);
  }
}

// ---- systems ----

ecs_system(hud_msg_request_system, EcsIn(HudMsgRequest), EcsResInOut(HudMessages)) {
  HudMessages *h = ecs_sys_get_resource_mut(HudMessages);
  for (i32 i = 0; i < it->count; i++) {
    const HudMsgRequest *r = ecs_sys_get_component(HudMsgRequest, i);
    u32 c = (u32)r->channel;
    if (r->clear) {
      hud_msg_clear_channel(h, c);
    } else if (r->msg.delay > 0.0f) {
      HudMsgChannelState *s = &h->channels[c];
      if (s->pending_len < HUD_MSG_QUEUE_MAX)
        s->pending[s->pending_len++] = (HudMsgPending){.msg = r->msg, .wait = r->msg.delay};
      else
        log_warn("hud_msg: channel delay list full, message dropped");
    } else {
      hud_msg_submit(h, &r->msg);
    }
    ecs_entity_destroy(it->world, it->entities[i]);
  }
}

ecs_system(hud_msg_tick_system, EcsResInOut(HudMessages)) {
  HudMessages *h = ecs_sys_get_resource_mut(HudMessages);
  for (u32 c = 0; c < HUD_MSG_CHANNEL_COUNT; c++)
    hud_msg_tick_channel(h, c, it->delta_time);
}

void hud_msg_init(EcsWorld *w, const HudMsgDesc *desc) {
  ecs_resource_set_zero(w, HudMessages);
  HudMessages *h = ecs_resource_get_mut(w, HudMessages);
  if (desc)
    for (u32 c = 0; c < HUD_MSG_CHANNEL_COUNT; c++)
      h->desc[c] = desc->channels[c];
}

void hud_msg_schedule(EcsWorld *w) {
  ecs_schedule_single(w, hud_msg_request_system);
  ecs_schedule_single(w, hud_msg_tick_system);
}

void hud_msg_push(EcsWorld *w, const HudMsg *msg) {
  assert_msg((u32)msg->channel < HUD_MSG_CHANNEL_COUNT && msg->kind < HUD_MSG_KINDS, "hud_msg_push: channel or kind out of range");
  EcsEntity e = ecs_entity_create(w);
  ecs_component_set(w, e, HudMsgRequest, {.channel = msg->channel, .msg = *msg});
}

void hud_msg_clear(EcsWorld *w, HudMsgChannel channel) {
  EcsEntity e = ecs_entity_create(w);
  ecs_component_set(w, e, HudMsgRequest, {.clear = true, .channel = channel});
}

b32 hud_msg_view(const HudMessages *h, HudMsgChannel channel, HudMsgView *out) {
  const HudMsgChannelState *s = &h->channels[channel];
  if (s->phase == HUD_MSG_PHASE_IDLE)
    return false;
  *out = (HudMsgView){.msg = &s->active, .phase = s->phase, .level = s->level, .serial = s->serial};
  return true;
}

// ---- layout ----

hz_internal HzUIString hud_msg_upper(const char *chars, u32 len) {
  Allocator ta = tctx_temp_allocator(NULL);
  char *out = ALLOC_ARRAY_NO_ZERO(&ta, char, len + 1);
  for (u32 i = 0; i < len; i++)
    out[i] = char_to_upper(chars[i]);
  return (HzUIString){.chars = out, .length = (i32)len};
}

hz_internal HzUIString hud_msg_str(const char *chars, u32 len, b32 upper) {
  return upper ? hud_msg_upper(chars, len) : (HzUIString){.chars = chars, .length = (i32)len};
}

// one line: a plain text child, or a row of runs when '*' marks emphasis
hz_internal hz_noinline void hud_msg_line(const char *chars, u32 len, b32 upper, const HzUITextConfig *base,
                                          const HzUITextConfig *emph, u32 id) {
  b32 marked = false;
  for (u32 i = 0; i < len; i++)
    marked = marked || chars[i] == '*';
  if (!marked) {
    ui_text(hud_msg_str(chars, len, upper), *base);
    return;
  }
  ui_element({.id = ui_idi("HudMsgLine", id), .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {
    u32 start = 0;
    b32 in_emph = false;
    for (u32 i = 0; i <= len; i++) {
      if (i < len && chars[i] != '*')
        continue;
      if (i > start)
        ui_text(hud_msg_str(chars + start, i - start, upper), in_emph && emph->fontSize > 0.0f ? *emph : *base);
      in_emph = !in_emph;
      start = i + 1;
    }
  }
}

hz_internal hz_noinline void hud_msg_text(const char *chars, u32 len, b32 upper, const HzUITextConfig *base,
                                          const HzUITextConfig *emph, u32 id) {
  u32 start = 0, line = 0;
  for (u32 i = 0; i <= len; i++) {
    if (i < len && chars[i] != '\n')
      continue;
    hud_msg_line(chars + start, i - start, upper, base, emph, id * 8u + line++);
    start = i + 1;
  }
}

hz_internal hz_noinline void hud_msg_body(const HudMsg *m, const HudMsgStyle *s, u32 id) {
  const HudMsgKindStyle *k = &s->kinds[m->kind];
  const HzUITextConfig *title = k->title.fontSize > 0.0f ? &k->title : &s->title;
  const HzUITextConfig *body = k->body.fontSize > 0.0f ? &k->body : &s->body;
  ui_element({.id = ui_idi("HudMsgText", id),
              .layout = {.layout_direction = UI_TOP_TO_BOTTOM,
                         .child_alignment = {.x = title->textAlignment == UI_TEXT_ALIGN_CENTER ? UI_ALIGN_X_CENTER : UI_ALIGN_X_LEFT}},
              .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {
    hud_msg_text(m->title.value, m->title.len, s->title_upper, title, &s->emph, id * 2u);
    if (m->body.len > 0) {
      ui_element({.layout = {.margin = {.top = (u16)s->body_gap}}, .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {
        hud_msg_text(m->body.value, m->body.len, s->body_upper, body, &s->emph, id * 2u + 1u);
      }
    }
  }
}

hz_internal hz_noinline void hud_msg_key(const HudMsg *m, const HudMsgStyle *s) {
  ui_element({.layout = {.sizing = {ui_sizing_fixed(s->key_size.x), ui_sizing_fixed(s->key_size.y)},
                         .child_alignment = {UI_ALIGN_X_CENTER, UI_ALIGN_Y_CENTER}},
              .shadow = {.color = s->key_ring, .spread = 1.5f, .inset = true},
              .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {
    ui_text(hud_msg_str(m->key.value, m->key.len, false), s->key);
  }
}

void hud_msg_layout(const HudMessages *h, UISystem *ui, HudMsgChannel channel, const HudMsgStyle *s) {
  HudMsgView v;
  if (!hud_msg_view(h, channel, &v) || v.level <= 0.0f)
    return;
  f32 ease = v.phase == HUD_MSG_PHASE_EXIT ? 1.0f - ease_css_ease(1.0f - v.level) : ease_css_ease(v.level);
  f32 alpha = ease, scale = 1.0f;
  v2 slide = {0.0f, 0.0f};
  if (s->anim == HUD_MSG_ANIM_SLIDE) {
    slide = v2_scale(s->slide_px, 1.0f - ease);
  } else if (s->anim == HUD_MSG_ANIM_POP) {
    f32 k = ease_cubic_bezier(0.2f, 0.9f, 0.3f, 1.2f, v.level);
    alpha = m_saturatef(k);
    scale = m_lerpf(s->pop_scale > 0.0f ? s->pop_scale : 1.0f, 1.0f, k);
  }
  v2 at = {s->offset.x + s->offset_screen.x * (f32)ui->canvas_width, s->offset.y + s->offset_screen.y * (f32)ui->canvas_height};
  f32 max_w = s->max_width;
  ui_element({.id = ui_idi("HudMsg", (u32)channel),
              .layout = {.sizing = {ui_sizing_fit(0, max_w), ui_sizing_fit(0)},
                         .padding = s->padding,
                         .child_gap = (u16)s->key_gap,
                         .child_alignment = {.y = UI_ALIGN_Y_CENTER}},
              .floating = {.attach_to = UI_ATTACH_TO_ROOT,
                           .attach_points = {.element = s->attach, .parent = s->parent},
                           .offset = at,
                           .z_index = s->z_index,
                           .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH},
              .background_color = s->background,
              .corner_radius = ui_corner_radius(s->corner_radius),
              .border = s->border,
              .backdrop_blur = s->backdrop_blur,
              .shadow = s->shadow,
              .opacity = opt_some(f32, alpha),
              .transform = opt_some(HzUITransform, ((HzUITransform){.translate = slide, .scale = {scale, scale}, .skew = {s->skew_x, 0.0f}})),
              .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {
    if (v.msg->key.len > 0)
      hud_msg_key(v.msg, s);
    hud_msg_body(v.msg, s, (u32)channel);
  }
}
