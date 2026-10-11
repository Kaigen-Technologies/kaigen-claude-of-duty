// Claude of Duty HUD widgets: helpers and the centre layer; sizes are the CSS numbers times k, colours sRGB bytes.
#include "shaders/cod_ui_band.h"
#include "shaders/cod_ui_hurt.h"
#include "shaders/cod_ui_minimap.h"

#define COD_PASS UI_POINTER_CAPTURE_MODE_PASSTHROUGH
#define COD_Z_HURT 10
#define COD_Z_WORLD 11
#define COD_Z_CENTRE 12
#define COD_Z_CHROME 13
#define COD_Z_MENU 20
#define COD_Z_LOADING 30

#define COD_INK ((HzUIColor){238, 244, 247, 242.25f})
#define COD_INK2 ((HzUIColor){214, 227, 234, 153.0f})
#define COD_INK3 ((HzUIColor){196, 210, 219, 76.5f})
#define COD_HAIR ((HzUIColor){255, 255, 255, 38.25f})
#define COD_AMBER ((HzUIColor){255, 176, 42, 255})
#define COD_RED ((HzUIColor){255, 63, 49, 255})
#define COD_CYAN ((HzUIColor){121, 210, 255, 255})
#define COD_FRIEND ((HzUIColor){143, 200, 255, 255})
#define COD_ENEMY ((HzUIColor){255, 122, 99, 255})
#define COD_OUTLINE ((HzUIColor){8, 12, 16, 255})
#define COD_WHITE ((HzUIColor){255, 255, 255, 255})
#define COD_CLEAR ((HzUIColor){0, 0, 0, 0})

// the pad: 6.5 u, a u being 4 px
#define COD_PAD 26.0f

// the frame counter panel, in u = 1080p px; the killfeed stacks below it
#define COD_FPS_W 210.0f
#define COD_FPS_H 112.0f

typedef enum CodFont {
  COD_FONT_BODY = 0,
  COD_FONT_DISPLAY,
  COD_FONT_MONO,
} CodFont;

hz_internal HzUIColor cod_alpha(HzUIColor c, f32 a) { return (HzUIColor){c.r, c.g, c.b, c.a * a}; }

hz_internal HzUIColor cod_rgba(f32 r, f32 g, f32 b, f32 a) { return (HzUIColor){r, g, b, a * 255.0f}; }

// the easing curves that the engine's differ from
hz_internal f32 cod_ease_out_back(f32 t) {
  f32 c = 1.9f, u = t - 1.0f;
  return 1.0f + (c + 1.0f) * u * u * u + c * u * u;
}

// the first dead slot from the cursor, else the oldest by life fraction
hz_internal CodHudPoolItem *cod_hud_pool_acquire(CodHudPoolItem *items, u32 count, u32 *next) {
  CodHudPoolItem *best = &items[0];
  f32 best_age = -1.0e30f;
  for (u32 i = 0; i < count; i++) {
    u32 at = (*next + i) % count;
    CodHudPoolItem *it = &items[at];
    if (!it->alive) {
      *next = (at + 1) % count;
      it->alive = true;
      it->t = 0.0f;
      return it;
    }
    f32 age = it->t / (it->life > 0.0f ? it->life : 1.0f);
    if (age > best_age) {
      best_age = age;
      best = it;
    }
  }
  best->alive = true;
  best->t = 0.0f;
  return best;
}

// ---- layout helpers ----

hz_internal HzUIFloating cod_float_root(HzUIFloatingAttachPointType point, v2 offset, i16 z) {
  return (HzUIFloating){.attach_to = UI_ATTACH_TO_ROOT,
                        .attach_points = {.element = point, .parent = point},
                        .offset = offset,
                        .z_index = z,
                        .pointer_capture_mode = COD_PASS};
}

// the element's point on a point of the canvas, plus an offset
hz_internal HzUIFloating cod_float_root_at(HzUIFloatingAttachPointType element, HzUIFloatingAttachPointType parent, v2 offset, i16 z) {
  return (HzUIFloating){.attach_to = UI_ATTACH_TO_ROOT,
                        .attach_points = {.element = element, .parent = parent},
                        .offset = offset,
                        .z_index = z,
                        .pointer_capture_mode = COD_PASS};
}

// the element's point at an absolute canvas position
hz_internal HzUIFloating cod_float_abs(HzUIFloatingAttachPointType element, v2 pos, i16 z) {
  return (HzUIFloating){.attach_to = UI_ATTACH_TO_ROOT,
                        .attach_points = {.element = element, .parent = UI_ATTACH_POINT_LEFT_TOP},
                        .offset = pos,
                        .z_index = z,
                        .pointer_capture_mode = COD_PASS};
}

// the element's point on the canvas centre plus an offset
hz_internal HzUIFloating cod_float_centre(HzUIFloatingAttachPointType element, v2 offset, i16 z) {
  return (HzUIFloating){.attach_to = UI_ATTACH_TO_ROOT,
                        .attach_points = {.element = element, .parent = UI_ATTACH_POINT_CENTER_CENTER},
                        .offset = offset,
                        .z_index = z,
                        .pointer_capture_mode = COD_PASS};
}

// the element's centre on a point of its parent (z inherits)
hz_internal HzUIFloating cod_float_parent(HzUIFloatingAttachPointType element, HzUIFloatingAttachPointType parent, v2 offset) {
  return (HzUIFloating){.attach_to = UI_ATTACH_TO_PARENT,
                        .attach_points = {.element = element, .parent = parent},
                        .offset = offset,
                        .pointer_capture_mode = COD_PASS};
}

// a full-canvas layer; its opacity fades everything declared inside
hz_internal HzUIFloating cod_float_layer(i16 z) { return cod_float_root(UI_ATTACH_POINT_LEFT_TOP, (v2){0, 0}, z); }

hz_internal HzUISizing cod_fixed2(f32 w, f32 h) { return (HzUISizing){ui_sizing_fixed(w), ui_sizing_fixed(h)}; }

// ---- text ----

hz_internal HzHandleT(UIFontFamily) cod_family(const CodUi *u, CodFont f) {
  return f == COD_FONT_DISPLAY ? u->display : f == COD_FONT_MONO ? u->mono : u->body;
}

hz_internal HzUITextConfig cod_text(const CodUi *u, CodFont font, f32 size, u16 weight, HzUIColor color, f32 spacing) {
  return (HzUITextConfig){.font_family = cod_family(u, font),
                          .weight = weight,
                          .fontSize = size,
                          .textColor = color,
                          .letterSpacing = spacing,
                          .wrapMode = UI_TEXT_WRAP_NONE};
}

// --sh: 0 1px 2px rgba(0,0,0,.92), 0 0 10k rgba(0,0,0,.45)
hz_internal HzUITextConfig cod_sh(HzUITextConfig c, f32 k) {
  c.shadows[0] = (HzUITextShadow){.color = cod_rgba(0, 0, 0, 0.92f), .offset_y = 1.0f, .blur = 2.0f};
  c.shadows[1] = (HzUITextShadow){.color = cod_rgba(0, 0, 0, 0.45f), .blur = 10.0f * k};
  return c;
}

// --sh-hard: 0 1px 1px rgba(0,0,0,.95)
hz_internal HzUITextConfig cod_sh_hard(HzUITextConfig c) {
  c.shadows[0] = (HzUITextShadow){.color = cod_rgba(0, 0, 0, 0.95f), .offset_y = 1.0f, .blur = 1.0f};
  return c;
}

// --sh-o1 / --sh-o2: a drawn outline of `ring` px (direction free) and a tight soft seat
hz_internal HzUITextConfig cod_outline(HzUITextConfig c, f32 ring, f32 seat_blur, f32 seat_alpha) {
  c.outlineColor = COD_OUTLINE;
  c.outlineWidth = (u16)m_maxf(1.0f, m_floorf(ring + 0.25f));
  c.shadows[0] = (HzUITextShadow){.color = cod_rgba(3, 6, 9, seat_alpha), .blur = seat_blur};
  return c;
}

hz_internal HzUITextConfig cod_o1(HzUITextConfig c, f32 k) { return cod_outline(c, 1.5f * k, 4.0f * k, 0.8f); }

hz_internal HzUITextConfig cod_o2(HzUITextConfig c, f32 k) { return cod_outline(c, 2.0f * k, 5.0f * k, 0.85f); }

// text with a bloom under its outline: `--o2, 0 0 16k rgba(0,0,0,.55)`
hz_internal HzUITextConfig cod_o2_bloom(HzUITextConfig c, f32 k, f32 bloom, f32 alpha) {
  c = cod_outline(c, 2.0f * k, bloom * k, alpha);
  return c;
}

// ui_str_c copies into frame memory and takes a mutable CString; every HUD string is read-only
hz_internal HzUIString cod_ui_str(const char *s) { return ui_str_c((CString)s); }

// a frame-lifetime copy of a formatted number
hz_internal HzUIString cod_str_u32(u32 v) {
  String32Bytes s = {0};
  cstr_format(s.value, sizeof(s.value), "%", fmt_u32(v));
  return cod_ui_str(s.value);
}

// ---- icon shapes: a box in a viewBox's units (scaled by s about (ox, oy)) on the parent ----

hz_internal hz_noinline void cod_shape_box(f32 s, v2 o, f32 x, f32 y, f32 w, f32 h, HzUIColor color, f32 radius) {
  ui_element({.layout = {.sizing = cod_fixed2(w * s, h * s)},
              .floating = cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){o.x + x * s, o.y + y * s}),
              .background_color = color,
              .corner_radius = ui_corner_radius(radius * s),
              .pointer_capture_mode = COD_PASS}) {}
}

// a polygon in the viewBox's units; points are absolute viewBox coordinates
hz_internal hz_noinline void cod_shape_poly_n(f32 s, v2 o, const v2 *pts, u32 n, HzUIColor color) {
  v2 lo = pts[0], hi = pts[0];
  for (u32 i = 1; i < n; i++) {
    lo = (v2){m_minf(lo.x, pts[i].x), m_minf(lo.y, pts[i].y)};
    hi = (v2){m_maxf(hi.x, pts[i].x), m_maxf(hi.y, pts[i].y)};
  }
  HzUIClipPath clip = {.count = n};
  for (u32 i = 0; i < n && i < UI_CLIP_PATH_MAX_POINTS; i++)
    clip.points[i] = (HzUIClipPoint){.offset = {(pts[i].x - lo.x) * s, (pts[i].y - lo.y) * s}};
  ui_element({.layout = {.sizing = cod_fixed2((hi.x - lo.x) * s, (hi.y - lo.y) * s)},
              .floating = cod_float_parent(UI_ATTACH_POINT_LEFT_TOP, UI_ATTACH_POINT_LEFT_TOP, (v2){o.x + lo.x * s, o.y + lo.y * s}),
              .background_color = color,
              .clip_path = clip,
              .pointer_capture_mode = COD_PASS}) {}
}

hz_internal void cod_shape_poly4(f32 s, v2 o, v2 a, v2 b, v2 c, v2 d, HzUIColor color) {
  Allocator ta = tctx_temp_allocator(NULL);
  v2 *p = ALLOC_ARRAY(&ta, v2, 4);
  p[0] = a;
  p[1] = b;
  p[2] = c;
  p[3] = d;
  cod_shape_poly_n(s, o, p, 4, color);
}

hz_internal void cod_shape_poly8(f32 s, v2 o, v2 a, v2 b, v2 c, v2 d, v2 e, v2 f, v2 g, v2 h, HzUIColor color) {
  Allocator ta = tctx_temp_allocator(NULL);
  v2 *p = ALLOC_ARRAY(&ta, v2, 8);
  p[0] = a;
  p[1] = b;
  p[2] = c;
  p[3] = d;
  p[4] = e;
  p[5] = f;
  p[6] = g;
  p[7] = h;
  cod_shape_poly_n(s, o, p, 8, color);
}

// ---- the crosshair ----

hz_internal void cod_hud_crosshair_update(CodHudFx *fx, const CodHudModel *m, f32 dt, f32 k) {
  // spring kick
  fx->kick_vel += (0.0f - fx->kick) * 150.0f * dt - fx->kick_vel * 15.0f * dt;
  fx->kick += fx->kick_vel * dt;
  if (fx->kick < 0.0f) {
    fx->kick = 0.0f;
    if (fx->kick_vel < 0.0f)
      fx->kick_vel *= 0.4f;
  }
  // movement and stance bloom
  f32 target = m->move * 7.0f + (m->sprint ? 6.0f : 0.0f) - (m->crouch ? 1.6f : 0.0f) + (m->airborne ? 5.0f : 0.0f);
  fx->move_spread = m_damp(fx->move_spread, target, 9.0f, dt);
  fx->ads_blend = m_damp(fx->ads_blend, m->ads ? 1.0f : 0.0f, 16.0f, dt);
  fx->hit_pulse = m_maxf(0.0f, fx->hit_pulse - dt * 5.5f);

  f32 base = m->base_spread - fx->ads_blend * 2.0f;
  fx->cross_gap = (base + fx->move_spread + fx->kick) * k;
  fx->cross_len = m_clampf(1.0f + fx->move_spread * 0.035f + fx->kick * 0.05f, 1.0f, 1.7f);
  f32 fade = m_saturatef(1.0f - fx->ads_blend * 1.25f);
  fx->visible = m_damp(fx->visible, fade, 22.0f, dt);
  fx->cross_bright = 1.0f - 0.25f * fx->ads_blend + 0.5f * ease_quad_out(fx->hit_pulse);
}

hz_internal hz_noinline void cod_hud_draw_crosshair(const CodHudFx *fx, f32 k) {
  f32 vis = fx->visible;
  if (vis < 0.004f)
    return;
  for (u32 i = 0; i < 4; i++) {
    f32 rot = (f32)i * 90.0f, rad = m_radians(rot);
    v2 at = {fx->cross_gap * m_sinf(rad), -fx->cross_gap * m_cosf(rad)};
    ui_element({.layout = {.sizing = cod_fixed2(1.6f * k, 8.0f * k)},
                .floating = cod_float_centre(UI_ATTACH_POINT_CENTER_CENTER, at, COD_Z_CENTRE),
                .gradient = {.kind = UI_GRADIENT_LINEAR,
                             .angle = 0.0f,
                             .stop_count = 2,
                             .stops = {{0.0f, {255, 255, 255, 158.1f}}, {0.62f, {255, 255, 255, 255}}}},
                .shadow = {.color = cod_rgba(0, 0, 0, 0.55f), .spread = 1.0f},
                .shadows = {{.color = cod_rgba(0, 0, 0, 0.75f), .blur = 3.0f * k}},
                .transform = opt_some(HzUITransform, ((HzUITransform){.rotation = rot, .scale = {1.0f, fx->cross_len}})),
                .opacity = opt_some(f32, vis * m_minf(1.0f, fx->cross_bright)),
                .pointer_capture_mode = COD_PASS}) {}
  }
  f32 dot = 1.0f + fx->hit_pulse * 1.1f;
  ui_element({.layout = {.sizing = cod_fixed2(2.2f * k, 2.2f * k)},
              .floating = cod_float_centre(UI_ATTACH_POINT_CENTER_CENTER, (v2){0, 0}, COD_Z_CENTRE),
              .background_color = COD_WHITE,
              .corner_radius = ui_corner_radius(1.1f * k),
              .shadow = {.color = cod_rgba(0, 0, 0, 0.6f), .spread = 1.0f},
              .shadows = {{.color = cod_rgba(0, 0, 0, 0.7f), .blur = 4.0f * k}},
              .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {dot, dot}})),
              .opacity = opt_some(f32, vis * (0.85f + 0.15f * fx->hit_pulse)),
              .pointer_capture_mode = COD_PASS}) {}
}

// ---- hitmarkers ----

typedef struct CodHitStyle {
  HzUIColor color;
  f32 weight, scale, life, ring, spin;
} CodHitStyle;

hz_internal CodHitStyle cod_hit_style(u32 kind) {
  switch (kind) {
  case COD_HIT_ARMOUR: return (CodHitStyle){{143, 220, 255, 255}, 2.0f, 1.03f, 0.28f, 0.5f, 0.0f};
  case COD_HIT_HEAD: return (CodHitStyle){{255, 194, 71, 255}, 2.2f, 1.08f, 0.32f, 0.3f, 0.0f};
  case COD_HIT_KILL: return (CodHitStyle){{255, 68, 51, 255}, 2.7f, 1.18f, 0.42f, 1.0f, 9.0f};
  default: return (CodHitStyle){{246, 250, 252, 255}, 1.8f, 1.0f, 0.26f, 0.0f, 0.0f};
  }
}

hz_internal void cod_hud_hit_spawn(CodHudFx *fx, u32 kind) {
  CodHitStyle s = cod_hit_style(kind);
  CodHudPoolItem *it = cod_hud_pool_acquire(fx->hits, COD_HUD_HITS, &fx->hits_next);
  it->life = s.life;
  it->kind = kind;
  fx->hit_pulse = 1.0f;
}

hz_internal void cod_hud_hits_update(CodHudFx *fx, f32 dt) {
  for (u32 i = 0; i < COD_HUD_HITS; i++) {
    CodHudPoolItem *it = &fx->hits[i];
    if (!it->alive)
      continue;
    it->t += dt;
    if (it->t >= it->life)
      it->alive = false;
  }
}

hz_internal hz_noinline void cod_hud_draw_hitmarker(const CodHudPoolItem *it, f32 k) {
  CodHitStyle s = cod_hit_style(it->kind);
  f32 u = it->t / it->life;
  f32 in_t = m_saturatef(u / 0.34f);
  f32 scale = s.scale * (0.62f + 0.38f * cod_ease_out_back(in_t));
  f32 alpha = u < 0.55f ? 1.0f : 1.0f - ease_sine_inout((u - 0.55f) / 0.45f);
  f32 rot = s.spin * ease_cubic_out(in_t);
  ui_element({.layout = {.sizing = cod_fixed2(56.0f * k, 56.0f * k)},
              .floating = cod_float_centre(UI_ATTACH_POINT_CENTER_CENTER, (v2){0, 0}, COD_Z_CENTRE),
              .transform = opt_some(HzUITransform, ((HzUITransform){.rotation = rot, .scale = {scale, scale}})),
              .opacity = opt_some(f32, alpha),
              .pointer_capture_mode = COD_PASS}) {
    if (s.ring > 0.0f) {
      f32 rt = m_saturatef(u / 0.6f);
      f32 rs = 0.55f + 1.15f * ease_quint_out(rt);
      f32 ro = s.ring * (1.0f - ease_quad_out(rt));
      f32 d = 61.4f * k;
      ui_element({.layout = {.sizing = cod_fixed2(d, d)},
                  .floating = cod_float_parent(UI_ATTACH_POINT_CENTER_CENTER, UI_ATTACH_POINT_CENTER_CENTER, (v2){0, 0}),
                  .corner_radius = ui_corner_radius(d * 0.5f),
                  .shadow = {.color = s.color, .spread = 1.4f * k, .inset = true},
                  .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {rs, rs}})),
                  .opacity = opt_some(f32, ro),
                  .pointer_capture_mode = COD_PASS}) {}
    }
    // the four diagonal strokes: a dark backing first, then the coloured stroke, square caps
    for (u32 layer = 0; layer < 2; layer++) {
      f32 w = layer == 0 ? 4.0f : s.weight;
      HzUIColor color = layer == 0 ? cod_rgba(0, 0, 0, 0.7f) : s.color;
      f32 len = (28.5f - 13.0f) + w;
      f32 mid = 20.75f * 0.70710678f;
      for (u32 q = 0; q < 4; q++) {
        f32 sx = (q == 0 || q == 3) ? 1.0f : -1.0f, sy = q < 2 ? -1.0f : 1.0f;
        ui_element({.layout = {.sizing = cod_fixed2(len * k, w * k)},
                    .floating = cod_float_parent(UI_ATTACH_POINT_CENTER_CENTER, UI_ATTACH_POINT_CENTER_CENTER,
                                                 (v2){mid * sx * k, mid * sy * k}),
                    .background_color = color,
                    .transform = opt_some(HzUITransform, ((HzUITransform){.rotation = sx * sy < 0.0f ? -45.0f : 45.0f})),
                    .pointer_capture_mode = COD_PASS}) {}
      }
    }
  }
}

// ---- directional damage arcs ----

hz_internal void cod_hud_arc_spawn(CodHudFx *fx, f32 dx, f32 dz, f32 intensity) {
  f32 len = m_sqrtf(dx * dx + dz * dz);
  if (len < 1e-6f)
    len = 1.0f;
  CodHudPoolItem *it = cod_hud_pool_acquire(fx->arcs, COD_HUD_ARCS, &fx->arcs_next);
  it->life = 2.0f;
  it->a = dx / len;
  it->b = dz / len;
  it->c = m_saturatef(intensity);
}

hz_internal void cod_hud_arcs_update(CodHudFx *fx, f32 dt) {
  for (u32 i = 0; i < COD_HUD_ARCS; i++) {
    CodHudPoolItem *it = &fx->arcs[i];
    if (!it->alive)
      continue;
    it->t += dt;
    if (it->t >= it->life)
      it->alive = false;
  }
}

// the segment weights across the arc
hz_internal f32 cod_arc_bell(u32 i) {
  switch (i) {
  case 0:
  case 6: return 0.1f;
  case 1:
  case 5: return 0.28f;
  case 2:
  case 4: return 0.62f;
  default: return 1.0f;
  }
}

hz_internal hz_noinline void cod_hud_draw_arc(const CodHudPoolItem *it, const CodHudModel *m, f32 k) {
  f32 u = it->t / it->life;
  f32 deg = m_degrees(m_atan2f(it->a * m->rx + it->b * m->rz, it->a * m->fx + it->b * m->fz));
  f32 in_t = m_saturatef(it->t / 0.09f);
  f32 scale = 0.92f + 0.08f * ease_quint_out(in_t);
  f32 hold = m_saturatef((u - 0.18f) / 0.82f);
  f32 alpha = (0.35f + 0.65f * it->c) * (1.0f - ease_quad_in(hold)) * ease_quad_out(in_t);
  ui_element({.layout = {.sizing = cod_fixed2(340.0f * k, 340.0f * k)},
              .floating = cod_float_centre(UI_ATTACH_POINT_CENTER_CENTER, (v2){0, 0}, COD_Z_CENTRE),
              .transform = opt_some(HzUITransform, ((HzUITransform){.rotation = deg, .scale = {scale, scale}})),
              .opacity = opt_some(f32, alpha),
              .pointer_capture_mode = COD_PASS}) {
    // seven stepped segments on a bell curve, three strokes: a dark backing, the arc, a thin outer line
    for (u32 layer = 0; layer < 3; layer++) {
      f32 stroke = layer == 0 ? 9.5f : layer == 1 ? 5.6f : 1.5f;
      f32 radius = layer == 2 ? 124.0f : 112.0f;
      HzUIColor base = layer == 0 ? (HzUIColor){0, 0, 0, 255} : layer == 1 ? (HzUIColor){255, 63, 49, 255} : (HzUIColor){255, 106, 82, 255};
      f32 scale_a = layer == 0 ? 0.5f * 0.9f : layer == 1 ? 1.0f : 0.4f;
      f32 half = m_radians(8.9f * 0.5f);
      f32 chord = 2.0f * radius * m_sinf(half);
      f32 mid_r = radius * m_cosf(half);
      for (u32 i = 0; i < 7; i++) {
        f32 c = ((f32)i - 3.0f) * 8.2f, cr = m_radians(c);
        ui_element({.layout = {.sizing = cod_fixed2(chord * k, stroke * k)},
                    .floating = cod_float_parent(UI_ATTACH_POINT_CENTER_CENTER, UI_ATTACH_POINT_CENTER_CENTER,
                                                 (v2){mid_r * m_sinf(cr) * k, -mid_r * m_cosf(cr) * k}),
                    .background_color = cod_alpha(base, cod_arc_bell(i) * scale_a),
                    .transform = opt_some(HzUITransform, ((HzUITransform){.rotation = c})),
                    .pointer_capture_mode = COD_PASS}) {}
      }
    }
  }
}
