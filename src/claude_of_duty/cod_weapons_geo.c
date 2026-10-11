// Claude of Duty weapons: the hard-surface kit and Assembly, the model finish and the contact AO bake.
#include "cod_weapons_geo.h"

// ------------------------------------------------------------------ pieces ---

PgMesh *cwg_mesh(Cwg *c) {
  PgMesh *m = ARENA_ALLOC(c->scratch, PgMesh);
  pg_mesh_begin(m, c->scratch, 64, 192);
  return m;
}

PgMesh *cwg_clone(Cwg *c, const PgMesh *src) {
  PgMesh *m = cwg_mesh(c);
  pg_mesh_append(m, src);
  return m;
}

PgMesh *cwg_merge(Cwg *c, PgMesh *const *list, u32 count) {
  PgMesh *m = cwg_mesh(c);
  for (u32 i = 0; i < count; i++)
    if (list[i])
      pg_mesh_append(m, list[i]);
  return m;
}

void cwg_translate(PgMesh *m, f32 x, f32 y, f32 z) { pg_mesh_translate(m, (v3){x, y, z}, 0); }

hz_internal void cwg_rotate(PgMesh *m, v3 axis, f32 a) {
  pg_mesh_transform(m, m4_trs(V3_ZERO, quat_axis_angle(axis, a), (v3){1.0f, 1.0f, 1.0f}), 0);
}

void cwg_rotate_x(PgMesh *m, f32 a) { cwg_rotate(m, (v3){1.0f, 0.0f, 0.0f}, a); }
void cwg_rotate_y(PgMesh *m, f32 a) { cwg_rotate(m, (v3){0.0f, 1.0f, 0.0f}, a); }
void cwg_rotate_z(PgMesh *m, f32 a) { cwg_rotate(m, (v3){0.0f, 0.0f, 1.0f}, a); }

void cwg_scale(PgMesh *m, f32 x, f32 y, f32 z) { pg_mesh_transform(m, m4_scale((v3){x, y, z}), 0); }

// chamfered box; a negligible chamfer is a plain BoxGeometry
PgMesh *cwg_box(Cwg *c, f32 w, f32 h, f32 d, f32 chamfer, u32 seg) {
  PgMesh *m = cwg_mesh(c);
  f32 r = m_minf(chamfer, m_minf(w, m_minf(h, d)) * 0.49f);
  if (r <= 1e-5f)
    pg_box_grid(m, (v3){w, h, d}, 1, 1, 1);
  else
    pg_round_box(m, (v3){w, h, d}, seg, r);
  return m;
}

PgMesh *cwg_box3(Cwg *c, f32 w, f32 h, f32 d) { return cwg_box(c, w, h, d, 0.0012f, 1); }

PgMesh *cwg_blob(Cwg *c, f32 w, f32 h, f32 d, f32 radius, u32 seg) { return cwg_box(c, w, h, d, radius, seg); }

// lathe around Z; the profile is (axial, radius) pairs and the form is built around +Y then turned onto +Z
PgMesh *cwg_lathe_zv(Cwg *c, const v2 *profile, u32 count, u32 seg, f32 phi_start, f32 phi_length) {
  PgMesh *m = cwg_mesh(c);
  Allocator al = make_arena_allocator(c->scratch);
  v2 *pts = ALLOC_ARRAY_NO_ZERO(&al, v2, count);
  for (u32 i = 0; i < count; i++)
    pts[i] = (v2){m_maxf(1e-5f, profile[i].y), profile[i].x};
  pg_revolve(m, pts, count, seg, phi_start, phi_length);
  cwg_rotate_x(m, M_PI * 0.5f);
  return m;
}

PgMesh *cwg_tube_z(Cwg *c, f32 r_outer, f32 r_inner, f32 len, u32 seg, f32 crown) {
  f32 z0 = -len * 0.5f, z1 = len * 0.5f;
  f32 cr = m_minf(crown, (r_outer - r_inner) * 0.4f);
  return cwg_lathe_z(c, seg, {z0 + cr, r_inner}, {z0, r_inner + cr}, {z0, r_outer - cr}, {z0 + cr, r_outer}, {z1 - cr, r_outer},
                     {z1, r_outer - cr}, {z1, r_inner + cr}, {z1 - cr, r_inner});
}

PgMesh *cwg_rod_z(Cwg *c, f32 r0, f32 r1, f32 len, u32 seg, f32 chamfer) {
  f32 z0 = -len * 0.5f, z1 = len * 0.5f;
  f32 cr = m_minf(m_minf(chamfer, len * 0.4f), m_minf(r0, r1) * 0.5f);
  return cwg_lathe_z(c, seg, {z0, 0.0f}, {z0, r0 - cr}, {z0 + cr, r0}, {z1 - cr, r1}, {z1, r1 - cr}, {z1, 0.0f});
}

PgMesh *cwg_dome(Cwg *c, f32 r, u32 seg, f32 cut) {
  PgMesh *m = cwg_mesh(c);
  u32 hs = (u32)m_roundf((f32)seg * 0.5f);
  pg_sphere(m, &(PgSphereDesc){.radius = r, .width_segments = seg, .height_segments = hs > 4 ? hs : 4, .phi_start = 0.0f,
                               .phi_length = 2.0f * M_PI, .theta_start = 0.0f, .theta_length = M_PI * cut});
  cwg_rotate_x(m, M_PI * 0.5f);
  return m;
}

// four corner arcs, seg + 1 points each; returns 4 * (seg + 1)
u32 cwg_round_rect(v2 *out, f32 w, f32 h, f32 r, u32 seg) {
  f32 hw = w * 0.5f - r, hh = h * 0.5f - r;
  u32 n = 0;
  for (u32 k = 0; k < 4; k++) {
    f32 cx = (k == 0 || k == 3) ? hw : -hw;
    f32 cy = k < 2 ? hh : -hh;
    f32 a0 = k == 0 ? 0.0f : k == 1 ? M_PI * 0.5f : k == 2 ? M_PI : -M_PI * 0.5f;
    for (u32 i = 0; i <= seg; i++) {
      f32 a = a0 + ((f32)i / (f32)seg) * (M_PI * 0.5f);
      out[n++] = (v2){cx + m_cosf(a) * r, cy + m_sinf(a) * r};
    }
  }
  return n;
}

// outline in XY extruded along Z with a bevel on both faces, centred on z = 0
PgMesh *cwg_extrude(Cwg *c, const v2 *pts, u32 count, f32 depth, f32 bevel, u32 bevel_segments, const PgPoly *holes, u32 hole_count) {
  PgMesh *m = cwg_mesh(c);
  Allocator al = make_arena_allocator(c->scratch);
  d2 *p = ALLOC_ARRAY_NO_ZERO(&al, d2, count);
  for (u32 i = 0; i < count; i++)
    p[i] = (d2){pts[i].x, pts[i].y};
  PgShape shape;
  pg_shape_begin(&shape, &al);
  pg_path2_from_points(&shape.path, p, count);
  for (u32 h = 0; h < hole_count; h++) {
    d2 *hp = ALLOC_ARRAY_NO_ZERO(&al, d2, holes[h].count);
    for (u32 i = 0; i < holes[h].count; i++)
      hp[i] = (d2){holes[h].pts[i].x, holes[h].pts[i].y};
    pg_path2_from_points(pg_shape_add_hole(&shape), hp, holes[h].count);
  }
  b32 bev = bevel > 1e-6f;
  pg_extrude_shape(m, &shape, 1,
                   &(PgExtrudeShapeDesc){.depth = m_maxf(1e-4f, depth - bevel * 2.0f), .bevel_enabled = bev, .bevel_thickness = bevel,
                                         .bevel_size = bevel, .bevel_segments = bev ? (bevel_segments ? bevel_segments : 1u) : 0u,
                                         .curve_segments = 6, .steps = 1});
  cwg_translate(m, 0.0f, 0.0f, -depth * 0.5f + bevel);
  return m;
}

PgPoly cwg_rr(Cwg *c, f32 w, f32 h, f32 r, u32 seg) {
  Allocator al = make_arena_allocator(c->scratch);
  v2 *pts = ALLOC_ARRAY_NO_ZERO(&al, v2, 4 * (seg + 1));
  u32 n = cwg_round_rect(pts, w, h, r, seg);
  return (PgPoly){pts, n};
}

PgMesh *cwg_ext_rr(Cwg *c, f32 w, f32 h, f32 r, u32 seg, f32 depth, f32 bevel) {
  PgPoly p = cwg_rr(c, w, h, r, seg);
  return cwg_extrude(c, p.pts, p.count, depth, bevel, 1, NULL, 0);
}

PgMesh *cwg_ring(Cwg *c, f32 radius, f32 thickness, u32 seg, u32 rings) {
  PgMesh *m = cwg_mesh(c);
  pg_torus(m, &(PgTorusDesc){.radius = radius, .tube = thickness, .radial_segments = rings, .tubular_segments = seg,
                             .arc = 2.0f * M_PI, .theta_start = 0.0f, .theta_length = 2.0f * M_PI});
  return m;
}

// hex-socket cap screw, axis +Z, head at z = 0 facing -Z, the socket a real counterbore
PgMesh *cwg_screw(Cwg *c, f32 r_head, f32 r_shank, f32 head_h, f32 shank_l, u32 seg) {
  f32 r_socket = r_head * 0.52f;
  PgMesh *head = cwg_lathe_z(c, seg, {0.0f, r_socket}, {0.0f, r_head - 0.0002f}, {0.0002f, r_head}, {head_h, r_head}, {head_h, r_shank},
                             {head_h + shank_l, r_shank}, {head_h + shank_l, 0.0f});
  PgMesh *bore = cwg_lathe_z(c, 6, {head_h * 0.62f, 0.0f}, {head_h * 0.62f, r_socket}, {0.0f, r_socket});
  PgMesh *both[2] = {head, bore};
  return cwg_merge(c, both, 2);
}

// a band of tiny pyramids around a cylinder
PgMesh *cwg_knurl_band(Cwg *c, f32 radius, f32 len, u32 count, f32 depth, u32 rows) {
  PgMesh *cell = cwg_mesh(c);
  pg_polyhedron(cell, PG_POLYHEDRON_OCTA, depth * 2.2f, 0);
  cwg_scale(cell, 1.0f, 1.0f, 0.55f);
  PgMesh *out = cwg_mesh(c);
  for (u32 r = 0; r < rows; r++) {
    f32 z = -len * 0.5f + (((f32)r + 0.5f) / (f32)rows) * len;
    for (u32 i = 0; i < count; i++) {
      f32 a = ((f32)i / (f32)count) * 2.0f * M_PI + (f32)(r % 2u) * (M_PI / (f32)count);
      u32 from = out->pos.len;
      pg_mesh_append(out, cell);
      pg_mesh_transform(out, m4_trs((v3){m_cosf(a) * radius, m_sinf(a) * radius, z}, quat_axis_angle((v3){0.0f, 0.0f, 1.0f}, a), (v3){1.0f, 1.0f, 1.0f}),
                        from);
    }
  }
  return out;
}

// fine longitudinal ribs
PgMesh *cwg_serrations(Cwg *c, f32 w, f32 h, f32 len, u32 count, f32 depth, char axis) {
  b32 ax = axis == 'x';
  f32 step = (ax ? w : h) / (f32)count;
  PgMesh *rib = cwg_box(c, ax ? step * 0.55f : w, ax ? h : step * 0.55f, len, depth * 0.9f, 1);
  PgMesh *out = cwg_mesh(c);
  for (u32 i = 0; i < count; i++) {
    f32 t = -0.5f + ((f32)i + 0.5f) / (f32)count;
    u32 from = out->pos.len;
    pg_mesh_append(out, rib);
    pg_mesh_translate(out, ax ? (v3){t * w, 0.0f, 0.0f} : (v3){0.0f, t * h, 0.0f}, from);
  }
  return out;
}

// MIL-STD-1913 rail along Z: a base plate and a run of chamfered crown blocks with real recoil slots between them
PgMesh *cwg_picatinny(Cwg *c, f32 len, const CwgPicatinny *o) {
  f32 width = o && o->width > 0.0f ? o->width : 0.0212f;
  f32 waist = o && o->waist > 0.0f ? o->waist : 0.0157f;
  f32 base_h = o && o->base_h > 0.0f ? o->base_h : 0.0042f;
  f32 top_h = o && o->top_h > 0.0f ? o->top_h : 0.0032f;
  f32 pitch = o && o->pitch > 0.0f ? o->pitch : 0.01055f;
  f32 slot = o && o->slot > 0.0f ? o->slot : 0.00535f;
  f32 ch = o && o->crown_chamfer > 0.0f ? o->crown_chamfer : 0.0015f;
  f32 chamfer = 0.00035f;
  f32 teeth_f = m_floorf((len + slot) / pitch);
  u32 teeth = teeth_f < 1.0f ? 1u : (u32)teeth_f;
  f32 tooth_len = pitch - slot;
  PgMesh *out = cwg_mesh(c);
  PgMesh *base = cwg_box(c, width, base_h, len, chamfer, 1);
  cwg_translate(base, 0.0f, base_h * 0.5f, 0.0f);
  pg_mesh_append(out, base);
  Allocator al = make_arena_allocator(c->scratch);
  v2 *profile = ALLOC_ARRAY_NO_ZERO(&al, v2, 6);
  profile[0] = (v2){-waist * 0.5f, 0.0f};
  profile[1] = (v2){-width * 0.5f, top_h - ch};
  profile[2] = (v2){-width * 0.5f + ch, top_h};
  profile[3] = (v2){width * 0.5f - ch, top_h};
  profile[4] = (v2){width * 0.5f, top_h - ch};
  profile[5] = (v2){waist * 0.5f, 0.0f};
  PgMesh *tooth = cwg_extrude(c, profile, 6, tooth_len, 0.00025f, 1, NULL, 0);
  for (u32 i = 0; i < teeth; i++) {
    f32 z = len * 0.5f - tooth_len * 0.5f - (f32)i * pitch;
    if (z - tooth_len * 0.5f < -len * 0.5f)
      break;
    u32 from = out->pos.len;
    pg_mesh_append(out, tooth);
    pg_mesh_translate(out, (v3){0.0f, base_h, z}, from);
  }
  return out;
}

// M-LOK slot: a recessed pocket with a raised lip
PgMesh *cwg_mlok_slot(Cwg *c, f32 len, f32 wide, f32 depth) {
  Allocator al = make_arena_allocator(c->scratch);
  v2 *outer_pts = ALLOC_ARRAY_NO_ZERO(&al, v2, 16);
  u32 no = cwg_round_rect(outer_pts, len, wide + 0.0028f, 0.0014f, 3);
  PgMesh *outer = cwg_extrude(c, outer_pts, no, 0.0016f, 0.0004f, 1, NULL, 0);
  v2 *inner_pts = ALLOC_ARRAY_NO_ZERO(&al, v2, 16);
  u32 ni = cwg_round_rect(inner_pts, len - 0.0016f, wide, 0.0012f, 3);
  PgMesh *inner = cwg_extrude(c, inner_pts, ni, depth, 0.0003f, 1, NULL, 0);
  cwg_translate(inner, 0.0f, 0.0f, -depth * 0.35f);
  PgMesh *both[2] = {outer, inner};
  return cwg_merge(c, both, 2);
}

// ---------------------------------------------------------------- assembly ---

hz_internal PgMesh *cwg_bucket(Cwg *c, CwpAsm *a, u32 mat) {
  PgMesh *b = &a->mesh[mat];
  if (!a->used[mat]) {
    pg_mesh_begin(b, c->out, 1024, 3072);
    a->used[mat] = true;
  }
  return b;
}

// Assembly.add: the piece transformed into assembly space and appended to its material's bucket
void cwg_add(Cwg *c, CwpAsm *a, const PgMesh *g, u32 mat, CwgT t) {
  PgMesh *b = cwg_bucket(c, a, mat);
  v3 s = {t.sx != 0.0f ? t.sx : 1.0f, t.sy != 0.0f ? t.sy : 1.0f, t.sz != 0.0f ? t.sz : 1.0f};
  u32 from = b->pos.len, ifrom = b->idx.len;
  pg_mesh_append(b, g);
  pg_mesh_transform(b, m4_trs((v3){t.x, t.y, t.z}, quat_from_euler((v3){t.rx, t.ry, t.rz}), s), from);
  if (s.x * s.y * s.z < 0.0f) {
    for (u32 i = ifrom; i + 2 < b->idx.len; i += 3) {
      u32 tmp = b->idx.items[i];
      b->idx.items[i] = b->idx.items[i + 2];
      b->idx.items[i + 2] = tmp;
    }
    for (u32 i = from; i < b->pos.len; i++)
      b->nrm.items[i] = v3_scale(b->nrm.items[i], -1.0f);
  }
}

void cwg_add_mirrored(Cwg *c, CwpAsm *a, const PgMesh *g, u32 mat, CwgT t) {
  cwg_add(c, a, g, mat, t);
  CwgT m = t;
  m.x = -t.x;
  m.sx = -(t.sx != 0.0f ? t.sx : 1.0f);
  cwg_add(c, a, g, mat, m);
}

// merges vertices at 1e-6: identical position, normal and uv collapse; collapsed triangles go
hz_internal void cwg_weld_exact(PgMesh *m) {
  u32 n = m->pos.len;
  if (n == 0)
    return;
  tctx_temp_allocator_scope(ta, m->arena) {
    Allocator *al = &ta.allocator;
    u32 cap = 64;
    while (cap < n * 2)
      cap <<= 1;
    u32 *table = ALLOC_ARRAY_NO_ZERO(al, u32, cap);
    for (u32 i = 0; i < cap; i++)
      table[i] = ~0u;
    u32 *remap = ALLOC_ARRAY_NO_ZERO(al, u32, n);
    i32 *key = ALLOC_ARRAY_NO_ZERO(al, i32, (u64)n * 8);
    u32 kept = 0;
    for (u32 i = 0; i < n; i++) {
      v3 p = m->pos.items[i], q = m->nrm.items[i];
      v2 u = m->uv.items[i];
      i32 *k = key + (u64)kept * 8;
      k[0] = (i32)m_roundf(p.x * 1e6f);
      k[1] = (i32)m_roundf(p.y * 1e6f);
      k[2] = (i32)m_roundf(p.z * 1e6f);
      k[3] = (i32)m_roundf(q.x * 1e6f);
      k[4] = (i32)m_roundf(q.y * 1e6f);
      k[5] = (i32)m_roundf(q.z * 1e6f);
      k[6] = (i32)m_roundf(u.x * 1e6f);
      k[7] = (i32)m_roundf(u.y * 1e6f);
      u32 h = 2166136261u;
      for (u32 j = 0; j < 8; j++)
        h = (h ^ (u32)k[j]) * 16777619u;
      u32 s = h & (cap - 1);
      u32 found = ~0u;
      while (table[s] != ~0u) {
        const i32 *o = key + (u64)table[s] * 8;
        b32 same = true;
        for (u32 j = 0; j < 8; j++)
          same = same && o[j] == k[j];
        if (same) {
          found = table[s];
          break;
        }
        s = (s + 1) & (cap - 1);
      }
      if (found == ~0u) {
        found = kept++;
        table[s] = found;
        m->pos.items[found] = p;
        m->nrm.items[found] = q;
        m->uv.items[found] = u;
      }
      remap[i] = found;
    }
    u32 out_idx = 0;
    for (u32 t = 0; t + 2 < m->idx.len; t += 3) {
      u32 a = remap[m->idx.items[t]], b = remap[m->idx.items[t + 1]], c = remap[m->idx.items[t + 2]];
      if (a == b || b == c || c == a)
        continue;
      m->idx.items[out_idx++] = a;
      m->idx.items[out_idx++] = b;
      m->idx.items[out_idx++] = c;
    }
    m->idx.len = out_idx;
    m->pos.len = kept;
    m->nrm.len = kept;
    m->uv.len = kept;
    m->mask.len = kept;
    m->charts.len = 0;
  }
}

// the linear convexity ramp raised to an exponent so it hugs the chamfer
hz_internal f32 cwg_shape1(f32 v, f32 amp, f32 exp) { return v <= 0.0f ? 0.0f : amp * m_powf(v > 1.0f ? 1.0f : v, exp); }

hz_internal void cwg_shape_masks(PgMesh *m, v3 amp, v3 exp) {
  for (u32 i = 0; i < m->col.len; i++) {
    v4 *c = &m->col.items[i];
    c->x = cwg_shape1(c->x, amp.x, exp.x);
    c->y = cwg_shape1(c->y, amp.y, exp.y);
    c->z = cwg_shape1(c->z, amp.z, exp.z);
  }
}

// merges every bucket's duplicate vertices, bakes the curvature masks and shapes them
void cwg_asm_finish(CwpAsm *a, f32 wear_scale, u32 seed, u32 *tris) {
  for (u32 mat = 0; mat < CWP_MAT_COUNT; mat++) {
    if (!a->used[mat])
      continue;
    PgMesh *m = &a->mesh[mat];
    cwg_weld_exact(m);
    if (m->idx.len == 0) {
      a->used[mat] = false;
      continue;
    }
    CodRng rng = cod_rng_make(seed + mat * 7919u);
    CodMaskOpts o = cod_mask_opts_default();
    o.edge_threshold = 0.16f;
    cod_bake_masks(m, &o, &rng);
    b32 soft = mat == CWP_MAT_POLYMER || mat == CWP_MAT_RUBBER || mat == CWP_MAT_POLYMER_TAN;
    cwg_shape_masks(m, (v3){(soft ? 0.42f : 0.62f) * wear_scale, 1.15f, 1.0f}, (v3){soft ? 3.4f : 2.8f, 1.25f, 1.15f});
    *tris += m->idx.len / 3;
  }
}

// only ever raises the AO channel near a contact
void cwg_bake_contact_ao(CwpModel *m, const v3 *contacts, u32 count, f32 radius, f32 peak) {
  if (count == 0)
    return;
  f32 r2 = radius * radius;
  for (u32 s = 0; s < 1u + CWP_PART_COUNT; s++) {
    CwpAsm *a = s == 0 ? &m->body : &m->part[s - 1];
    for (u32 mat = 0; mat < CWP_MAT_COUNT; mat++) {
      if (!a->used[mat])
        continue;
      PgMesh *g = &a->mesh[mat];
      if (!(g->streams & PG_STREAM_COLOR))
        continue;
      for (u32 i = 0; i < g->pos.len; i++) {
        f32 closest = FLT_MAX;
        for (u32 k = 0; k < count; k++)
          closest = m_minf(closest, v3_lengthsq(v3_sub(g->pos.items[i], contacts[k])));
        if (closest > r2)
          continue;
        f32 t = 1.0f - m_sqrtf(closest) / radius;
        f32 sm = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        g->col.items[i].z = m_maxf(g->col.items[i].z, peak * sm);
      }
    }
  }
}
