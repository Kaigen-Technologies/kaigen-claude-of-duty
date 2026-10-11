#include "hud_map.h"
#include "lib/thread_context.h"

HudMapPoint hud_map_project(const HudMap *m, v2 world_xz, b32 clamp) {
  v2 d = v2_scale(v2_sub(world_xz, m->center), m->px_per_m);
  if (m->rotate) {
    f32 c = m_cosf(m->heading), s = m_sinf(m->heading);
    d = (v2){d.x * c + d.y * s, -d.x * s + d.y * c};
  }
  HudMapPoint p = {.px = d, .angle = m_atan2f(d.y, d.x)};
  f32 hx = m_maxf(m->half_px.x - m->edge_margin, 1.0e-3f), hy = m_maxf(m->half_px.y - m->edge_margin, 1.0e-3f);
  f32 k;
  if (m->clip == HUD_MAP_CLIP_CIRCLE) {
    f32 nx = d.x / hx, ny = d.y / hy;
    k = m_sqrtf(nx * nx + ny * ny);
  } else {
    k = m_maxf(m_absf(d.x) / hx, m_absf(d.y) / hy);
  }
  p.inside = k <= 1.0f;
  if (!p.inside && clamp) {
    p.px = v2_scale(d, 1.0f / k);
    p.clamped = true;
  }
  return p;
}

f32 hud_bearing(v2 from, v2 to) { return m_atan2f(to.x - from.x, -(to.y - from.y)); }

b32 hud_compass_place(const HudCompass *c, f32 bearing, f32 heading, f32 *x, f32 *alpha) {
  f32 rel = m_wrap_pi(bearing - heading);
  if (m_absf(rel) >= c->span)
    return false;
  f32 px = c->width * 0.5f + rel / c->span * (c->width * 0.5f);
  f32 a = m_powf(m_maxf(0.0f, 1.0f - m_absf(rel) / c->span), c->fade_power);
  if (c->mask_edge > 0.0f) {
    f32 f = px / c->width;
    a *= m_saturatef(f / c->mask_edge) * m_saturatef((1.0f - f) / c->mask_edge);
  }
  *x = px;
  *alpha = a;
  return a > 0.0f;
}

HudMapFit hud_map_fit(const v2 *points, u32 count, f32 canvas_px, f32 pad_frac) {
  v2 lo = points[0], hi = points[0];
  for (u32 i = 1; i < count; i++) {
    lo = (v2){m_minf(lo.x, points[i].x), m_minf(lo.y, points[i].y)};
    hi = (v2){m_maxf(hi.x, points[i].x), m_maxf(hi.y, points[i].y)};
  }
  f32 pad = canvas_px * pad_frac;
  f32 scale = (canvas_px - pad * 2.0f) / m_maxf(m_maxf(hi.x - lo.x, hi.y - lo.y), 1.0e-3f);
  return (HudMapFit){.scale = scale,
                     .offset = {canvas_px * 0.5f - (lo.x + hi.x) * 0.5f * scale, canvas_px * 0.5f - (lo.y + hi.y) * 0.5f * scale}};
}

hz_internal f32 hud_map_seg_dist(v2 p, v2 a, v2 b) {
  v2 ab = v2_sub(b, a);
  f32 len2 = v2_dot(ab, ab);
  f32 t = len2 > 0.0f ? m_clampf(v2_dot(v2_sub(p, a), ab) / len2, 0.0f, 1.0f) : 0.0f;
  return v2_length(v2_sub(p, v2_add(a, v2_scale(ab, t))));
}

GpuTexture hud_map_bake(const v2 *points, u32 count, b32 closed, const HudMapFit *fit, u32 canvas_px, f32 range_px) {
  TempAllocator ta = tctx_temp_allocator_begin(NULL);
  u32 texel_count = canvas_px * canvas_px;
  f32 *field = ALLOC_ARRAY_NO_ZERO(&ta.allocator, f32, texel_count);
  for (u32 i = 0; i < texel_count; i++)
    field[i] = range_px;
  f32 reach = range_px * 0.5f;
  u32 segments = closed ? count : count - 1;
  for (u32 j = 0; j < segments; j++) {
    v2 pa = points[j], pb = points[(j + 1) % count];
    v2 a = {fit->offset.x + pa.x * fit->scale, fit->offset.y + pa.y * fit->scale};
    v2 b = {fit->offset.x + pb.x * fit->scale, fit->offset.y + pb.y * fit->scale};
    i32 x0 = (i32)m_maxf(0.0f, m_floorf(m_minf(a.x, b.x) - reach));
    i32 x1 = (i32)m_minf((f32)canvas_px - 1.0f, m_ceilf(m_maxf(a.x, b.x) + reach));
    i32 y0 = (i32)m_maxf(0.0f, m_floorf(m_minf(a.y, b.y) - reach));
    i32 y1 = (i32)m_minf((f32)canvas_px - 1.0f, m_ceilf(m_maxf(a.y, b.y) + reach));
    for (i32 y = y0; y <= y1; y++) {
      for (i32 x = x0; x <= x1; x++) {
        f32 *dst = &field[(u32)y * canvas_px + (u32)x];
        *dst = m_minf(*dst, hud_map_seg_dist((v2){(f32)x + 0.5f, (f32)y + 0.5f}, a, b));
      }
    }
  }
  u16 *texels = ALLOC_ARRAY_NO_ZERO(&ta.allocator, u16, texel_count);
  for (u32 i = 0; i < texel_count; i++)
    texels[i] = (u16)(m_saturatef(field[i] / range_px) * 65535.0f + 0.5f);
  GpuTexture tex = gpu_make_texture(&(GpuTextureDesc){.data = (u8 *)texels,
                                                       .size = texel_count * (u32)sizeof(u16),
                                                       .data_type = GPU_TEXTURE_DATA_R16,
                                                       .width = canvas_px,
                                                       .height = canvas_px});
  tctx_temp_allocator_end(ta);
  return tex;
}
