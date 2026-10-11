// Claude of Duty world geometry toolkit: noise, level matrices, vertex masks and the local-space meshes the kit is made of.

// a local-space mesh: position, normal, uv, mask colour (wear, grime, ao) and triangle indices, on the allocator it was begun with
typedef struct CwGeo {
  v3 *pos;
  v3 *nrm;
  v2 *uv;
  v3 *col;
  u32 *idx;
  u32 nv;
  u32 ni;
  u32 cap_v;
  u32 cap_i;
  b32 has_col; // a colour (mask) attribute was authored
} CwGeo;

// per-vertex mask painter: local-space position and normal in, mask channels in/out
typedef void (*CwPaintFn)(const void *ctx, v3 p, v3 n, f32 *out);

#define CW_TAU 6.28318530717958647692
#define CW_PI_F ((f32)M_PI)

// ------------------------------------------------------------------ noise ---

// deterministic 3D value hash in [0,1), the integer mix on the same quantised inputs
hz_internal f64 cw_hash3(f64 x, f64 y, f64 z) {
  u32 h = ((u32)(i32)m_floor(x * 1013.0 + 0.5) ^ 0x27d4eb2du) * 0x85ebca6bu;
  h = (h ^ (u32)(i32)m_floor(y * 1619.0 + 0.5)) * 0xc2b2ae35u;
  h = (h ^ (u32)(i32)m_floor(z * 31337.0 + 0.5)) * 0x27d4eb2fu;
  h ^= h >> 15;
  return (f64)h / 4294967296.0;
}

hz_internal f64 cw_fade(f64 t) { return t * t * (3.0 - 2.0 * t); }

hz_internal f64 cw_noise3(f64 x, f64 y, f64 z) {
  f64 xi = m_floor(x), yi = m_floor(y), zi = m_floor(z);
  f64 xf = cw_fade(x - xi), yf = cw_fade(y - yi), zf = cw_fade(z - zi);
  f64 acc = 0.0;
  for (i32 dz = 0; dz < 2; dz++) {
    f64 wz = dz ? zf : 1.0 - zf;
    for (i32 dy = 0; dy < 2; dy++) {
      f64 wy = dy ? yf : 1.0 - yf;
      for (i32 dx = 0; dx < 2; dx++) {
        f64 wx = dx ? xf : 1.0 - xf;
        acc += cw_hash3(xi + dx, yi + dy, zi + dz) * wx * wy * wz;
      }
    }
  }
  return acc;
}

hz_internal f32 cw_fbm3(f64 x, f64 y, f64 z, i32 octaves) {
  f64 a = 0.5, sum = 0.0, norm = 0.0;
  for (i32 i = 0; i < octaves; i++) {
    sum += cw_noise3(x, y, z) * a;
    norm += a;
    a *= 0.5;
    x *= 2.03;
    y *= 2.01;
    z *= 1.97;
  }
  return (f32)(sum / norm);
}

hz_internal f32 cw_cosh(f32 x) { return 0.5f * (m_expf(x) + m_expf(-x)); }
hz_internal f32 cw_clamp01(f32 x) { return m_clampf(x, 0.0f, 1.0f); }
hz_internal f32 cw_sign(f32 x) { return m_signf(x); }

// ----------------------------------------------------------------- matrix ---

// compose(position, Euler YXZ rotation (rx, ry, rz), scale)
hz_internal m4 cw_trs(f32 x, f32 y, f32 z, f32 ry, f32 sx, f32 sy, f32 sz, f32 rx, f32 rz) {
  f32 a = m_cosf(rx), b = m_sinf(rx), c = m_cosf(ry), d = m_sinf(ry), e = m_cosf(rz), f = m_sinf(rz);
  f32 ce = c * e, cf = c * f, de = d * e, df = d * f;
  m4 m;
  m.m[0][0] = (ce + df * b) * sx;
  m.m[0][1] = (a * f) * sx;
  m.m[0][2] = (cf * b - de) * sx;
  m.m[0][3] = 0.0f;
  m.m[1][0] = (de * b - cf) * sy;
  m.m[1][1] = (a * e) * sy;
  m.m[1][2] = (df + ce * b) * sy;
  m.m[1][3] = 0.0f;
  m.m[2][0] = (a * d) * sz;
  m.m[2][1] = (-b) * sz;
  m.m[2][2] = (a * c) * sz;
  m.m[2][3] = 0.0f;
  m.m[3][0] = x;
  m.m[3][1] = y;
  m.m[3][2] = z;
  m.m[3][3] = 1.0f;
  return m;
}

// pm * compose(...)
hz_internal m4 cw_ll(m4 pm, f32 x, f32 y, f32 z, f32 ry, f32 sx, f32 sy, f32 sz, f32 rx, f32 rz) {
  return m4_mul(pm, cw_trs(x, y, z, ry, sx, sy, sz, rx, rz));
}

// the level -> world transform: rotation about Y then translation
hz_internal m4 cw_level_xform(void) { return cw_trs(COD_LEVEL_TX, 0.0f, COD_LEVEL_TZ, COD_LEVEL_YAW, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f); }

hz_internal m3 cw_normal_matrix(m4 m) {
  m3 upper;
  for (u32 c = 0; c < 3; c++)
    for (u32 r = 0; r < 3; r++)
      upper.m[c][r] = m.m[c][r];
  return m3_transpose(m3_inv(upper));
}

// --------------------------------------------------------------- geometry ---

hz_internal CwGeo cw_geo_new(Allocator *al, u32 cap_v, u32 cap_i) {
  CwGeo g = {0};
  g.cap_v = cap_v;
  g.has_col = true;
  g.cap_i = cap_i;
  g.pos = ALLOC_ARRAY_NO_ZERO(al, v3, cap_v);
  g.nrm = ALLOC_ARRAY_NO_ZERO(al, v3, cap_v);
  g.uv = ALLOC_ARRAY_NO_ZERO(al, v2, cap_v);
  g.col = ALLOC_ARRAY_NO_ZERO(al, v3, cap_v);
  g.idx = ALLOC_ARRAY_NO_ZERO(al, u32, cap_i);
  return g;
}

hz_internal u32 cw_geo_vert(CwGeo *g, v3 p, v3 n, v2 uv, v3 c) {
  assert_msg(g->nv < g->cap_v, "cw geo: vertex capacity");
  u32 i = g->nv++;
  g->pos[i] = p;
  g->nrm[i] = n;
  g->uv[i] = uv;
  g->col[i] = c;
  return i;
}

hz_internal void cw_geo_tri(CwGeo *g, u32 a, u32 b, u32 c) {
  assert_msg(g->ni + 3 <= g->cap_i, "cw geo: index capacity");
  g->idx[g->ni++] = a;
  g->idx[g->ni++] = b;
  g->idx[g->ni++] = c;
}

hz_internal u32 cw_geo_tris(const CwGeo *g) { return g->ni / 3; }

hz_internal void cw_geo_fill_masks(CwGeo *g, f32 w, f32 gr, f32 a) {
  g->has_col = true;
  for (u32 i = 0; i < g->nv; i++)
    g->col[i] = (v3){w, gr, a};
}

// positions by m, normals by its normal matrix
hz_internal void cw_geo_apply(CwGeo *g, m4 m) {
  m3 nm = cw_normal_matrix(m);
  for (u32 i = 0; i < g->nv; i++) {
    g->pos[i] = m4_mulv3(m, g->pos[i], 1.0f);
    g->nrm[i] = v3_normalize(m3_mulv3(nm, g->nrm[i]));
  }
}

hz_internal void cw_geo_translate(CwGeo *g, f32 x, f32 y, f32 z) {
  for (u32 i = 0; i < g->nv; i++)
    g->pos[i] = v3_add(g->pos[i], (v3){x, y, z});
}

hz_internal aabb cw_geo_bounds(const CwGeo *g) {
  aabb b = aabb_invalidate();
  for (u32 i = 0; i < g->nv; i++)
    b = aabb_encapsulate_point(b, g->pos[i]);
  return b;
}

// merges two geometries: appends src (indices offset)
hz_internal void cw_geo_append(CwGeo *dst, const CwGeo *src) {
  assert_msg(dst->nv + src->nv <= dst->cap_v && dst->ni + src->ni <= dst->cap_i, "cw geo: merge capacity");
  u32 base = dst->nv;
  mem_cpy(dst->pos + base, src->pos, sizeof(v3) * src->nv);
  mem_cpy(dst->nrm + base, src->nrm, sizeof(v3) * src->nv);
  mem_cpy(dst->uv + base, src->uv, sizeof(v2) * src->nv);
  mem_cpy(dst->col + base, src->col, sizeof(v3) * src->nv);
  for (u32 i = 0; i < src->ni; i++)
    dst->idx[dst->ni + i] = base + src->idx[i];
  dst->nv += src->nv;
  dst->ni += src->ni;
}

// area-weighted over the index, flat where vertices are not shared
hz_internal void cw_geo_normals(CwGeo *g) {
  for (u32 i = 0; i < g->nv; i++)
    g->nrm[i] = V3_ZERO;
  for (u32 t = 0; t + 2 < g->ni; t += 3) {
    u32 a = g->idx[t], b = g->idx[t + 1], c = g->idx[t + 2];
    v3 cb = v3_sub(g->pos[c], g->pos[b]);
    v3 ab = v3_sub(g->pos[a], g->pos[b]);
    v3 n = v3_cross(cb, ab);
    g->nrm[a] = v3_add(g->nrm[a], n);
    g->nrm[b] = v3_add(g->nrm[b], n);
    g->nrm[c] = v3_add(g->nrm[c], n);
  }
  for (u32 i = 0; i < g->nv; i++) {
    f32 l = v3_length(g->nrm[i]);
    g->nrm[i] = l > 0.0f ? v3_scale(g->nrm[i], 1.0f / l) : g->nrm[i];
  }
}

// the callback edits each vertex's mask triple given its local position and normal
hz_internal void cw_geo_paint(CwGeo *g, CwPaintFn fn, const void *ctx) {
  g->has_col = true;
  for (u32 i = 0; i < g->nv; i++) {
    v3 out = g->col[i];
    fn(ctx, g->pos[i], g->nrm[i], &out.x);
    g->col[i] = out;
  }
}

// copies an engine PgMesh into a CwGeo (masks zero, as a three geometry without a colour attribute)
hz_internal CwGeo cw_geo_from_pg(Allocator *al, const PgMesh *m) {
  CwGeo g = cw_geo_new(al, m->pos.len, m->idx.len);
  g.has_col = false;
  for (u32 i = 0; i < m->pos.len; i++)
    cw_geo_vert(&g, m->pos.items[i], m->nrm.items[i], m->uv.items[i], V3_ZERO);
  for (u32 i = 0; i < m->idx.len; i++)
    g.idx[g.ni++] = m->idx.items[i];
  return g;
}

// ------------------------------------------------------------- primitives ---

// 6 inset faces, 12 edge strips and 8 corner triangles (44 tris), flat shaded with analytic wear masks
hz_internal CwGeo cw_chamfer_box(Allocator *al, f32 sx, f32 sy, f32 sz, f32 bevel) {
  f32 h[3] = {sx * 0.5f, sy * 0.5f, sz * 0.5f};
  f32 b = m_maxf(0.0005f, m_minf(bevel, m_minf(sx, m_minf(sy, sz)) * 0.4f));
  CwGeo g = cw_geo_new(al, 96, 132);
  f32 signs[8][3];
  for (u32 i = 0; i < 8; i++) {
    signs[i][0] = (i & 1) ? 1.0f : -1.0f;
    signs[i][1] = (i & 2) ? 1.0f : -1.0f;
    signs[i][2] = (i & 4) ? 1.0f : -1.0f;
  }
#define CW_CB_VERT(ci, axis, out)                                                                                          \
  do {                                                                                                                     \
    for (u32 a_ = 0; a_ < 3; a_++)                                                                                         \
      ((f32 *)&(out))[a_] = signs[ci][a_] * ((i32)a_ == (axis) ? h[a_] : h[a_] - b);                                       \
  } while (0)

  // a polygon of 3 or 4 points, wound outward from the box centre
  v3 pts[4];
  u32 pn;
  struct {
    f32 wear, grime;
  } style;
#define CW_CB_POLY()                                                                                                       \
  do {                                                                                                                     \
    v3 e1 = v3_sub(pts[1], pts[0]), e2 = v3_sub(pts[2], pts[0]);                                                           \
    v3 n0 = v3_cross(e1, e2);                                                                                              \
    v3 cen = V3_ZERO;                                                                                                      \
    for (u32 k = 0; k < pn; k++)                                                                                           \
      cen = v3_add(cen, pts[k]);                                                                                           \
    cen = v3_scale(cen, 1.0f / (f32)pn);                                                                                   \
    if (v3_dot(n0, cen) < 0.0f) {                                                                                          \
      for (u32 k = 0; k < pn / 2; k++) {                                                                                   \
        v3 t_ = pts[k];                                                                                                    \
        pts[k] = pts[pn - 1 - k];                                                                                          \
        pts[pn - 1 - k] = t_;                                                                                              \
      }                                                                                                                    \
    }                                                                                                                      \
    v3 n = v3_normalize(v3_cross(v3_sub(pts[1], pts[0]), v3_sub(pts[2], pts[0])));                                         \
    f32 ax_ = m_fabsf(n.x), ay_ = m_fabsf(n.y), az_ = m_fabsf(n.z);                                                        \
    i32 ax = ax_ > ay_ ? (ax_ > az_ ? 0 : 2) : (ay_ > az_ ? 1 : 2);                                                        \
    f32 gr = n.y < -0.5f ? style.grime + 0.35f : style.grime;                                                              \
    u32 first = g.nv;                                                                                                      \
    for (u32 k = 0; k < pn; k++) {                                                                                         \
      v2 uv = {ax == 0 ? pts[k].z : pts[k].x, ax == 1 ? pts[k].z : pts[k].y};                                              \
      cw_geo_vert(&g, pts[k], n, uv, (v3){style.wear, m_minf(1.0f, gr), n.y < -0.4f ? 0.35f : 0.0f});                       \
    }                                                                                                                      \
    for (u32 k = 1; k + 1 < pn; k++)                                                                                       \
      cw_geo_tri(&g, first, first + k, first + k + 1);                                                                     \
  } while (0)

  // 6 faces: the four corners of a face ordered round it by angle
  for (i32 axis = 0; axis < 3; axis++) {
    for (i32 si = 0; si < 2; si++) {
      f32 sa = si ? 1.0f : -1.0f;
      u32 corners[4];
      u32 cn = 0;
      for (u32 ci = 0; ci < 8; ci++)
        if (signs[ci][axis] == sa)
          corners[cn++] = ci;
      i32 a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
      f32 ang[4];
      for (u32 k = 0; k < 4; k++)
        ang[k] = m_atan2f(signs[corners[k]][a2], signs[corners[k]][a1]);
      for (u32 i = 1; i < 4; i++) {
        u32 c = corners[i];
        f32 an = ang[i];
        u32 j = i;
        while (j > 0 && ang[j - 1] > an) {
          corners[j] = corners[j - 1];
          ang[j] = ang[j - 1];
          j--;
        }
        corners[j] = c;
        ang[j] = an;
      }
      pn = 4;
      for (u32 k = 0; k < 4; k++)
        CW_CB_VERT(corners[k], axis, pts[k]);
      style.wear = 0.06f;
      style.grime = 0.0f;
      CW_CB_POLY();
    }
  }
  // 12 edge strips
  for (i32 a = 0; a < 3; a++) {
    for (i32 bx = a + 1; bx < 3; bx++) {
      for (i32 si = 0; si < 2; si++) {
        for (i32 sj = 0; sj < 2; sj++) {
          f32 sa = si ? 1.0f : -1.0f, sb = sj ? 1.0f : -1.0f;
          u32 cs[2];
          u32 cn = 0;
          for (u32 ci = 0; ci < 8; ci++)
            if (signs[ci][a] == sa && signs[ci][bx] == sb)
              cs[cn++] = ci;
          pn = 4;
          CW_CB_VERT(cs[0], a, pts[0]);
          CW_CB_VERT(cs[0], bx, pts[1]);
          CW_CB_VERT(cs[1], bx, pts[2]);
          CW_CB_VERT(cs[1], a, pts[3]);
          style.wear = 1.0f;
          style.grime = 0.0f;
          CW_CB_POLY();
        }
      }
    }
  }
  // 8 corner triangles
  for (u32 ci = 0; ci < 8; ci++) {
    pn = 3;
    CW_CB_VERT(ci, 0, pts[0]);
    CW_CB_VERT(ci, 1, pts[1]);
    CW_CB_VERT(ci, 2, pts[2]);
    style.wear = 1.0f;
    style.grime = 0.0f;
    CW_CB_POLY();
  }
#undef CW_CB_POLY
#undef CW_CB_VERT
  return g;
}

// a 12-tri unit box with zero masks
hz_internal CwGeo cw_plain_box(Allocator *al) {
  CwGeo g = cw_geo_new(al, 24, 36);
  for (i32 axis = 0; axis < 3; axis++) {
    for (i32 si = 0; si < 2; si++) {
      f32 s = si ? 1.0f : -1.0f;
      i32 a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
      v3 n = V3_ZERO;
      ((f32 *)&n)[axis] = s;
      u32 first = g.nv;
      for (u32 k = 0; k < 4; k++) {
        f32 u = (k == 1 || k == 2) ? 0.5f : -0.5f;
        f32 v = (k >= 2) ? 0.5f : -0.5f;
        v3 p = V3_ZERO;
        ((f32 *)&p)[axis] = 0.5f * s;
        ((f32 *)&p)[a1] = u;
        ((f32 *)&p)[a2] = v;
        cw_geo_vert(&g, p, n, (v2){u + 0.5f, v + 0.5f}, V3_ZERO);
      }
      v3 e1 = v3_sub(g.pos[first + 1], g.pos[first]), e2 = v3_sub(g.pos[first + 2], g.pos[first]);
      b32 flip = v3_dot(v3_cross(e1, e2), n) < 0.0f;
      cw_geo_tri(&g, first, flip ? first + 2 : first + 1, flip ? first + 1 : first + 2);
      cw_geo_tri(&g, first, flip ? first + 3 : first + 2, flip ? first + 2 : first + 3);
    }
  }
  return g;
}

// a single quad in XY facing +Z: window glass, thin panels
hz_internal CwGeo cw_quad(Allocator *al, f32 w, f32 h) {
  CwGeo g = cw_geo_new(al, 4, 6);
  v3 n = {0.0f, 0.0f, 1.0f};
  cw_geo_vert(&g, (v3){-w * 0.5f, h * 0.5f, 0.0f}, n, (v2){0.0f, 1.0f}, V3_ZERO);
  cw_geo_vert(&g, (v3){w * 0.5f, h * 0.5f, 0.0f}, n, (v2){1.0f, 1.0f}, V3_ZERO);
  cw_geo_vert(&g, (v3){-w * 0.5f, -h * 0.5f, 0.0f}, n, (v2){0.0f, 0.0f}, V3_ZERO);
  cw_geo_vert(&g, (v3){w * 0.5f, -h * 0.5f, 0.0f}, n, (v2){1.0f, 0.0f}, V3_ZERO);
  cw_geo_tri(&g, 0, 2, 1);
  cw_geo_tri(&g, 2, 3, 1);
  return g;
}

// CylinderGeometry along +Y translated up by height/2, masks zero
hz_internal CwGeo cw_cylinder(Allocator *al, f32 r_top, f32 r_bottom, f32 height, u32 radial, u32 segs, b32 open) {
  CwGeo g;
  ArenaAllocator *conflict = allocator_as_arena(al);
  tctx_temp_allocator_scope(ta, conflict) {
    PgMesh m;
    pg_mesh_begin(&m, ta.temp.arena, 64, 256);
    pg_cylinder(&m, &(PgCylinderDesc){.radius_top = r_top, .radius_bottom = r_bottom, .height = height, .radial_segments = radial,
                                      .height_segments = segs, .open_ended = open});
    g = cw_geo_from_pg(al, &m);
  }
  return g;
}

// a straight capped tube from y = 0 to y = height
hz_internal CwGeo cw_tube_y(Allocator *al, f32 radius, f32 height, u32 radial, f32 taper, b32 open, u32 seg) {
  CwGeo g = cw_cylinder(al, radius * taper, radius, height, radial, seg, open);
  cw_geo_translate(&g, 0.0f, height * 0.5f, 0.0f);
  return g;
}

// an icosahedron pushed about by fbm and squashed; faceted, flat normals
hz_internal CwGeo cw_rock(Allocator *al, f32 size, u32 detail, f32 squash, f32 seed) {
  CwGeo g;
  ArenaAllocator *conflict = allocator_as_arena(al);
  tctx_temp_allocator_scope(ta, conflict) {
    PgMesh m;
    pg_mesh_begin(&m, ta.temp.arena, 64, 256);
    pg_polyhedron(&m, PG_POLYHEDRON_ICOSA, size * 0.5f, detail);
    g = cw_geo_from_pg(al, &m);
  }
  for (u32 i = 0; i < g.nv; i++) {
    v3 p = g.pos[i];
    f32 n = cw_fbm3(p.x * 7.0f + seed, p.y * 7.0f + seed, p.z * 7.0f + seed, 2);
    f32 f = 0.62f + n * 0.72f;
    p = v3_scale(p, f);
    p.y *= squash;
    g.pos[i] = p;
  }
  cw_geo_normals(&g);
  return g;
}

// ------------------------------------------------------------------ patch ---

// a flat irregular fan on XZ (sand drifts, stains); the per-lobe radii were drawn by the caller
hz_internal CwGeo cw_patch(Allocator *al, const f32 *radii, u32 lobes, f32 sag) {
  CwGeo g = cw_geo_new(al, lobes + 1, lobes * 3);
  v3 n = {0.0f, 1.0f, 0.0f};
  cw_geo_vert(&g, V3_ZERO, n, (v2){0.0f, 0.0f}, V3_ZERO);
  for (u32 i = 0; i < lobes; i++) {
    f32 t = ((f32)i / (f32)lobes) * (f32)CW_TAU;
    cw_geo_vert(&g, (v3){m_cosf(t) * radii[i], -sag, m_sinf(t) * radii[i]}, n, (v2){m_cosf(t), m_sinf(t)}, V3_ZERO);
  }
  for (u32 i = 0; i < lobes; i++)
    cw_geo_tri(&g, 0, 1 + i, 1 + ((i + 1) % lobes));
  return g;
}

// ------------------------------------------------------------- drift berm ---

hz_internal CwGeo cw_drift_berm(Allocator *al, f32 len, f32 w, f32 h, u32 nz, f32 seed) {
  u32 nx = (u32)m_maxf(4.0f, m_roundf(len / 0.55f));
  u32 row = nz + 1;
  CwGeo g = cw_geo_new(al, (nx + 1) * row, nx * nz * 6);
  for (u32 i = 0; i <= nx; i++) {
    f32 u = (f32)i / (f32)nx;
    f32 x = (u - 0.5f) * len;
    f32 taper = m_minf(1.0f, m_minf(u, 1.0f - u) * 6.0f);
    f32 wob = 0.45f + cw_fbm3(x * 0.7f + seed, 2.1, seed, 3) * 1.1f;
    f32 ch = h * wob * taper;
    f32 cw = w * (0.6f + cw_fbm3(x * 0.5f + seed + 7.0f, 5.3, 1.9, 2) * 0.85f);
    for (u32 j = 0; j <= nz; j++) {
      f32 v = (f32)j / (f32)nz;
      // f32 cos(pi/2) is -4e-8, a NaN under pow
      f32 y = ch * m_powf(m_maxf(m_cosf((v * CW_PI_F) / 2.0f), 0.0f), 1.7f);
      f32 rip = cw_fbm3(x * 2.3f + seed, v * 3.1f, 8.4, 2) - 0.5f;
      cw_geo_vert(&g, (v3){x, m_maxf(0.0f, y + rip * h * 0.22f * (1.0f - v)), v * cw}, (v3){0, 1, 0}, (v2){x * 0.5f, v * cw * 0.5f}, V3_ZERO);
    }
  }
  for (u32 i = 0; i < nx; i++) {
    for (u32 j = 0; j < nz; j++) {
      u32 a = i * row + j;
      cw_geo_tri(&g, a, a + 1, a + row);
      cw_geo_tri(&g, a + 1, a + row + 1, a + row);
    }
  }
  cw_geo_normals(&g);
  return g;
}

// ----------------------------------------------------------------- runoff ---

// a rain-runoff stain strip hanging from y = 0 down to -len, grime masks feathered at every edge
hz_internal CwGeo cw_runoff(Allocator *al, f32 width, f32 len, f32 amount, u32 cols, u32 rows, f32 wander, f32 seed) {
  CwGeo g = cw_geo_new(al, (rows + 1) * (cols + 1), rows * cols * 6);
  for (u32 j = 0; j <= rows; j++) {
    f32 v = (f32)j / (f32)rows;
    f32 drift = (cw_fbm3(seed + v * 2.3f, 4.1, 1.7, 2) - 0.5f) * wander * width * v;
    f32 wj = width * (1.0f - v * 0.42f) * (0.85f + 0.3f * cw_fbm3(seed + 9.0f, v * 3.1f, 2.2, 2));
    for (u32 i = 0; i <= cols; i++) {
      f32 u = (f32)i / (f32)cols;
      f32 side = m_powf(m_maxf(m_sinf(CW_PI_F * u), 0.0f), 0.8f); // f32 sin(pi) is -9e-8
      f32 head = m_minf(1.0f, v / 0.10f);
      f32 tail = 1.0f - v * v;
      f32 broken = 0.55f + 0.75f * cw_fbm3(seed + u * 4.3f, v * 5.7f, 3.3, 2);
      f32 gr = m_minf(1.0f, amount * side * head * tail * broken);
      cw_geo_vert(&g, (v3){(u - 0.5f) * wj + drift, -v * len, 0.0f}, (v3){0, 0, 1}, (v2){u, v}, (v3){0.0f, gr, gr * 0.45f});
    }
  }
  u32 row = cols + 1;
  for (u32 j = 0; j < rows; j++) {
    for (u32 i = 0; i < cols; i++) {
      u32 a = j * row + i;
      cw_geo_tri(&g, a, a + row, a + 1);
      cw_geo_tri(&g, a + 1, a + row, a + row + 1);
    }
  }
  return g;
}

// ------------------------------------------------------------------ cloth ---

typedef struct CwClothSpec {
  f32 w, h;
  u32 seg_x, seg_y;
  f32 sag, wrinkle, twist, bulge;
  b32 has_range;
  f32 u0, u1;
  f32 seed;
  f32 thickness, hem, fray, bow;
} CwClothSpec;

hz_internal CwClothSpec cw_cloth_defaults(f32 w, f32 h) {
  return (CwClothSpec){.w = w, .h = h, .seg_x = 10, .seg_y = 8, .sag = 0.12f, .wrinkle = 0.03f, .u0 = 0.0f, .u1 = 1.0f,
                       .thickness = 0.0022f, .hem = 1.0f, .bow = 1.0f};
}

hz_internal f32 cw_tri_wave(f32 t) {
  f32 f = (f32)(m_fmod(m_fmod((f64)t, 1.0) + 1.0, 1.0));
  return f < 0.5f ? f * 4.0f - 1.0f : 3.0f - f * 4.0f;
}

// a real double shell offset along its own normal, closed by a hem rim
hz_internal CwGeo cw_cloth(Allocator *al, const CwClothSpec *s) {
  f32 w = s->w, h = s->h;
  u32 segX = s->seg_x, segY = s->seg_y;
  f32 u0 = s->u0, u1 = s->u1;
  f32 sw = w * (u1 - u0);
  u32 nx = segX + 1, ny = segY + 1, nv = nx * ny;
  u32 loop_n = 2 * (segX + segY);
  CwGeo g = cw_geo_new(al, nv * 2 + loop_n * 4, segX * segY * 12 + loop_n * 6);
  TempAllocator ta = tctx_temp_allocator_begin(allocator_as_arena(al));
  v3 *P = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, nv);
  v3 *N = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, nv);
  v2 *UV = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, nv);
  f32 *HT = ALLOC_ARRAY_NO_ZERO(&ta.allocator, f32, nv);
  f32 *AO = ALLOC_ARRAY_NO_ZERO(&ta.allocator, f32, nv);
  f32 seed = s->seed;
  for (u32 j = 0; j < ny; j++) {
    for (u32 i = 0; i < nx; i++) {
      u32 k = j * nx + i;
      f32 uu = (f32)i / (f32)segX;
      f32 u = u0 + uu * (u1 - u0);
      f32 v = (f32)j / (f32)segY;
      f32 x = (u - 0.5f) * w;
      f32 y = (v - 0.5f) * h;
      f32 cat = cw_cosh((u - 0.5f) * 2.2f) - 1.0f;
      f32 z = -cat * s->sag * (1.0f - v * 0.35f);
      z += m_sinf(v * 7.1f + u * 3.3f + seed) * s->wrinkle * (0.4f + u * (1.0f - u) * 3.0f);
      z += m_sinf(u * 11.3f + seed * 2.0f) * s->wrinkle * 0.5f * v;
      f32 cr = cw_tri_wave(u * 2.6f + v * 1.15f + seed * 0.37f);
      z += cr * s->wrinkle * 0.85f * (0.4f + 0.6f * (1.0f - v));
      z -= s->bulge * m_sinf(u * CW_PI_F) * m_sinf(v * CW_PI_F);
      z *= s->bow;
      y -= cat * s->sag * 0.5f;
      x += s->twist * (v - 0.5f) * m_sinf(u * 4.0f + seed);
      if (j == 0 && s->fray > 0.0f) {
        y -= s->fray * (0.3f + 0.7f * m_fabsf(m_sinf(u * 8.7f + seed * 1.7f)));
        x += s->fray * 0.35f * m_sinf(u * 15.3f + seed);
      }
      P[k] = (v3){x, y, z};
      UV[k] = (v2){uu, v};
      f32 dBottom = v * h, dTop = (1.0f - v) * h;
      f32 dSide = m_minf(uu, 1.0f - uu) * sw;
      f32 band = m_maxf(m_maxf(0.0f, 1.0f - dBottom / 0.045f), 0.55f * m_maxf(0.0f, 1.0f - m_minf(dTop, dSide) / 0.03f));
      HT[k] = s->thickness * 0.5f * (1.0f + s->hem * 2.8f * band * band);
      AO[k] = m_maxf(0.0f, -cr) * 0.4f + band * 0.3f;
    }
  }
  for (u32 j = 0; j < ny; j++) {
    for (u32 i = 0; i < nx; i++) {
      u32 k = j * nx + i;
      u32 i0 = i > 0 ? i - 1 : i, i1 = i < nx - 1 ? i + 1 : i;
      u32 j0 = j > 0 ? j - 1 : j, j1 = j < ny - 1 ? j + 1 : j;
      v3 tu = v3_sub(P[j * nx + i1], P[j * nx + i0]);
      v3 tv = v3_sub(P[j1 * nx + i], P[j0 * nx + i]);
      v3 n = {tu.y * tv.z - tu.z * tv.y, tu.z * tv.x - tu.x * tv.z, tu.x * tv.y - tu.y * tv.x};
      f32 l = v3_length(n);
      if (l == 0.0f)
        l = 1.0f;
      N[k] = v3_scale(n, 1.0f / l);
    }
  }
  for (u32 sh = 0; sh < 2; sh++) {
    f32 sign = sh == 0 ? 1.0f : -1.0f;
    u32 base = g.nv;
    for (u32 k = 0; k < nv; k++) {
      f32 o = HT[k] * sign;
      f32 grime = sh == 0 ? AO[k] * 0.5f : 0.28f + AO[k] * 0.5f;
      cw_geo_vert(&g, v3_add(P[k], v3_scale(N[k], o)), v3_scale(N[k], sign), UV[k], (v3){AO[k] * 0.5f, grime, AO[k] * (sh == 0 ? 1.0f : 1.3f)});
    }
    for (u32 j = 0; j < segY; j++) {
      for (u32 i = 0; i < segX; i++) {
        u32 a = base + j * nx + i, b = a + 1, c = a + nx, d = c + 1;
        if (sign > 0.0f) {
          cw_geo_tri(&g, a, b, d);
          cw_geo_tri(&g, a, d, c);
        } else {
          cw_geo_tri(&g, a, d, b);
          cw_geo_tri(&g, a, c, d);
        }
      }
    }
  }
  u32 *loop = ALLOC_ARRAY_NO_ZERO(&ta.allocator, u32, loop_n);
  u32 ln = 0;
  for (u32 i = 0; i < nx - 1; i++)
    loop[ln++] = i;
  for (u32 j = 0; j < ny - 1; j++)
    loop[ln++] = j * nx + (nx - 1);
  for (u32 i = nx - 1; i > 0; i--)
    loop[ln++] = (ny - 1) * nx + i;
  for (u32 j = ny - 1; j > 0; j--)
    loop[ln++] = j * nx;
  for (u32 e = 0; e < ln; e++) {
    u32 ka = loop[e], kb = loop[(e + 1) % ln];
    v3 ed = v3_sub(P[kb], P[ka]);
    v3 mn = v3_scale(v3_add(N[ka], N[kb]), 0.5f);
    v3 o = {ed.y * mn.z - ed.z * mn.y, ed.z * mn.x - ed.x * mn.z, ed.x * mn.y - ed.y * mn.x};
    f32 ol = v3_length(o);
    if (ol == 0.0f)
      ol = 1.0f;
    o = v3_scale(o, 1.0f / ol);
    u32 q[4];
    u32 ks[4] = {ka, ka, kb, kb};
    f32 sg[4] = {1.0f, -1.0f, -1.0f, 1.0f};
    for (u32 c = 0; c < 4; c++) {
      u32 k = ks[c];
      f32 off = HT[k] * sg[c];
      q[c] = cw_geo_vert(&g, v3_add(P[k], v3_scale(N[k], off)), o, UV[k], (v3){0.45f, 0.3f + AO[k] * 0.4f, m_minf(1.0f, AO[k] + 0.2f)});
    }
    cw_geo_tri(&g, q[0], q[1], q[2]);
    cw_geo_tri(&g, q[0], q[2], q[3]);
  }
  tctx_temp_allocator_end(ta);
  return g;
}

// ---------------------------------------------------------- catenary tube ---

// a Catmull-Rom curve through a sagging polyline, swept as a thin tube; the jitter was drawn by the caller as noise inputs
hz_internal CwGeo cw_catenary(Allocator *al, v3 from, v3 to, f32 sag, f32 radius, u32 seg, u32 radial, f32 jitter) {
  CwGeo g;
  ArenaAllocator *conflict = allocator_as_arena(al);
  tctx_temp_allocator_scope(ta, conflict) {
    v3 *pts = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, seg + 1);
    f32 K = cw_cosh(1.5f) - 1.0f;
    for (u32 i = 0; i <= seg; i++) {
      f32 t = (f32)i / (f32)seg;
      f32 droop = (cw_cosh(1.5f) - cw_cosh((t - 0.5f) * 3.0f)) / K;
      f32 jx = jitter != 0.0f ? (cw_fbm3((f64)i * 3.1, 1.2, 4.4, 2) - 0.5f) * jitter : 0.0f;
      f32 jz = jitter != 0.0f ? (cw_fbm3((f64)i * 2.7, 8.2, 1.4, 2) - 0.5f) * jitter : 0.0f;
      pts[i] = (v3){from.x + (to.x - from.x) * t + jx, from.y + (to.y - from.y) * t - sag * droop, from.z + (to.z - from.z) * t + jz};
    }
    PgCurve3 curve = pg_catmull_rom_curve(pts, seg + 1, false, PG_CATMULL_ROM_CENTRIPETAL, 0.0f);
    PgMesh m;
    pg_mesh_begin(&m, ta.temp.arena, 64, 256);
    pg_tube_curve(&m, &(PgTubeCurveDesc){.path = &curve, .tubular_segments = seg, .radius = radius, .radial_segments = radial, .closed = false});
    g = cw_geo_from_pg(al, &m);
  }
  return g;
}

// ------------------------------------------------------------------- sack ---

// a rounded-box Lp ball with ears, a seam and a tied neck; w/h/d are the finished dimensions
hz_internal CwGeo cw_sack(Allocator *al, f32 seed, f32 w, f32 h, f32 d, u32 variant, f32 box, f32 lump) {
  CwGeo g;
  ArenaAllocator *conflict = allocator_as_arena(al);
  tctx_temp_allocator_scope(ta, conflict) {
    PgMesh m;
    pg_mesh_begin(&m, ta.temp.arena, 64, 256);
    pg_sphere(&m, &(PgSphereDesc){.radius = 0.5f, .width_segments = 20, .height_segments = 12});
    g = cw_geo_from_pg(al, &m);
  }
  for (u32 i = 0; i < g.nv; i++) {
    v3 p = g.pos[i];
    f32 ux = p.x * 2.0f, uy = p.y * 2.0f, uz = p.z * 2.0f;
    f32 q = m_powf(m_fabsf(ux), box) + m_powf(m_fabsf(uy), box) + m_powf(m_fabsf(uz), box);
    f32 f = q > 1e-6f ? 1.0f / m_powf(q, 1.0f / box) : 1.0f;
    ux *= f;
    uy *= f;
    uz *= f;
    f32 flat = uy > 0.0f ? 1.0f - uy * uy * 0.2f : 1.0f;
    f32 n = cw_fbm3(ux * 3.4f + seed, uy * 3.4f + seed, uz * 3.4f + seed, 3) - 0.5f;
    f32 n2 = cw_fbm3(ux * 9.0f + seed * 2.0f, uy * 8.0f + seed, uz * 9.0f + seed * 3.0f, 2) - 0.5f;
    f32 x = ux * w * 0.5f * (1.0f + n * 0.09f * lump);
    f32 z = uz * d * 0.5f * flat * (1.0f + n * 0.26f * lump + n2 * 0.11f * lump);
    f32 y = uy * h * 0.5f * (1.0f + n * 0.24f * lump + n2 * 0.1f * lump);
    f32 t = x / (w * 0.5f);
    f32 neck = m_maxf(0.0f, m_fabsf(t) - 0.7f) / 0.3f;
    z *= 1.0f - neck * neck * 0.3f;
    y *= 1.0f - neck * neck * 0.55f;
    if (neck > 0.55f)
      y += cw_sign(uy) * h * 0.02f * (neck - 0.55f) * 2.0f;
    if (uy > 0.15f) {
      f32 zz = z / (d * 0.42f);
      y += h * 0.05f * m_expf(-(zz * zz) * 6.0f) * (1.0f - neck * 0.8f);
    }
    if (variant == 0) {
      z *= 1.0f + 0.06f * m_cosf(t * 2.6f);
    } else if (variant == 1) {
      x += w * 0.04f * t;
      f32 fatter = 1.0f + 0.13f * (0.5f - t);
      z *= fatter;
      f32 tt = t / 0.32f;
      y *= fatter * (1.0f - 0.16f * m_expf(-(tt * tt)));
      if (uy > 0.3f) {
        f32 t2 = t / 0.45f;
        y -= h * 0.05f * m_expf(-(t2 * t2));
      }
    } else {
      f32 cq = (t - 0.1f) / 0.16f;
      f32 crease = m_expf(-(cq * cq));
      z *= 1.0f - crease * 0.2f;
      y *= 1.0f - crease * 0.26f;
      if (t > 0.5f) {
        y *= 1.0f - (t - 0.5f) * 0.5f;
        z *= 1.0f + (t - 0.5f) * 0.22f;
      }
    }
    g.pos[i] = (v3){x, y, z};
  }
  cw_geo_normals(&g);
  return g;
}

// bends a long thin object's vertices by noise so it is not perfectly straight
hz_internal void cw_warp(CwGeo *g, f32 amp, f32 freq, f32 seed) {
  for (u32 i = 0; i < g->nv; i++) {
    v3 p = g->pos[i];
    f32 t = cw_fbm3(p.x * freq + seed, p.y * freq + seed * 1.7f, p.z * freq + seed * 2.3f, 2) - 0.5f;
    f32 t2 = cw_fbm3(p.z * freq + seed * 3.1f, p.y * freq, p.x * freq, 2) - 0.5f;
    g->pos[i] = (v3){p.x + t * amp, p.y + t2 * amp * 0.5f, p.z + t2 * amp};
  }
  cw_geo_normals(g);
}

// ------------------------------------------------------------- wall panel ---

typedef struct CwHole {
  f32 x, y, w, h;
  f32 arch;    // 0..1, a pointed arch head
  f32 ragged;  // > 0: a blown-out opening, amplitude
  const f32 *jitter; // ragged only: 18 draws
} CwHole;

typedef struct CwPanelSpec {
  f32 w, h, t;
  u32 hole_count;
  const CwHole *holes;
  f32 bevel;
  b32 top_ragged;
  f32 ragged_amp;
  const f32 *top_jitter; // ragged only: steps + 1 draws
  f32 jag;
  u32 curve_segments;
  f32 warp;
  u32 paint; // CwPaintKind of the merge-time painter
  f32 paint_p;
} CwPanelSpec;

// the wall panel paint: reveals get crevice grime and ao, the foot gets the dirt splash, chamfers wear
hz_internal void cw_panel_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)ctx;
  f32 face = m_fabsf(n.z);
  f32 reveal = 1.0f - face;
  f32 nz = cw_fbm3(p.x * 2.3f, p.y * 2.1f, p.z * 2.7f, 2);
  out[0] = m_minf(1.0f, reveal * 0.55f * (0.4f + nz) + m_maxf(0.0f, n.y) * 0.3f);
  out[1] = m_minf(1.0f, reveal * 0.42f * (0.5f + nz) + m_maxf(0.0f, -n.y) * 0.55f);
  out[2] = m_minf(1.0f, reveal * 0.4f + m_maxf(0.0f, -n.y) * 0.4f);
}

hz_internal u32 cw_ragged_steps(f32 w) { return (u32)m_maxf(4.0f, m_roundf(w / 0.55f)); }

// a slab with real openings extruded with chamfered reveals, bowed a few millimetres
hz_internal CwGeo cw_wall_panel(Allocator *al, const CwPanelSpec *s) {
  CwGeo g;
  ArenaAllocator *conflict = allocator_as_arena(al);
  f32 w = s->w, h = s->h, bevel = s->bevel;
  tctx_temp_allocator_scope(ta, conflict) {
    Allocator *sa = &ta.allocator;
    PgShape shape;
    pg_shape_begin(&shape, sa);
    PgPath2 *p = &shape.path;
    f64 x0 = -w * 0.5, x1 = w * 0.5;
    pg_path2_move_to(p, x0, 0.0);
    pg_path2_line_to(p, x1, 0.0);
    if (s->top_ragged) {
      u32 steps = cw_ragged_steps(w);
      f32 *py = ALLOC_ARRAY_NO_ZERO(sa, f32, steps + 1);
      f32 *px = ALLOC_ARRAY_NO_ZERO(sa, f32, steps + 1);
      for (u32 i = 0; i <= steps; i++) {
        f32 x = (f32)x1 - ((f32)i / (f32)steps) * w;
        f32 f = (f32)i / (f32)steps;
        f32 drop = s->ragged_amp * h * (0.25f + 0.75f * cw_fbm3(x * 0.6f + 11.0f, 3.1, 2.7, 3)) * (0.35f + f);
        px[i] = x;
        py[i] = m_maxf(0.4f, h - drop);
      }
      pg_path2_line_to(p, x1, py[0]);
      for (u32 i = 0; i <= steps; i++) {
        f32 nx = i < steps ? px[i + 1] : (f32)x0;
        pg_path2_line_to(p, px[i], py[i]);
        pg_path2_line_to(p, nx, py[i] + s->top_jitter[i]);
      }
      pg_path2_line_to(p, x0, py[steps]);
    } else {
      pg_path2_line_to(p, x1, h);
      if (s->jag > 0.0f) {
        u32 steps = (u32)m_maxf(3.0f, m_roundf(w / 1.2f));
        for (u32 i = steps - 1; i >= 1; i--) {
          f32 x = (f32)x0 + ((f32)i / (f32)steps) * w;
          pg_path2_line_to(p, x, h + (cw_fbm3(x * 1.7f, 5.5, 1.3, 2) - 0.5f) * s->jag);
        }
      }
      pg_path2_line_to(p, x0, h);
    }
    pg_path2_line_to(p, x0, 0.0);
    for (u32 hi = 0; hi < s->hole_count; hi++) {
      const CwHole *o = &s->holes[hi];
      PgPath2 *hp = pg_shape_add_hole(&shape);
      f64 hx0 = o->x - o->w / 2.0, hx1 = o->x + o->w / 2.0, hy0 = o->y - o->h / 2.0, hy1 = o->y + o->h / 2.0;
      if (o->ragged > 0.0f) {
        const u32 n = 18;
        for (u32 i = 0; i < n; i++) {
          f64 t = ((f64)i / (f64)n) * CW_TAU;
          f64 cx = (hx0 + hx1) / 2.0, cy = (hy0 + hy1) / 2.0, rx = (hx1 - hx0) / 2.0, ry = (hy1 - hy0) / 2.0;
          f64 c = m_cos(t), sn = m_sin(t);
          f64 k = 1.0 / m_pow(m_max(m_fabs(c), m_fabs(sn)), 0.85);
          f64 j = 1.0 + (f64)o->jitter[i];
          f64 px = cx + c * k * rx * j, py = cy + sn * k * ry * j;
          if (i == 0)
            pg_path2_move_to(hp, px, py);
          else
            pg_path2_line_to(hp, px, py);
        }
        pg_path2_close(hp);
      } else if (o->arch > 0.0f) {
        f64 r = (o->w / 2.0) * o->arch;
        f64 yA = hy1 - r;
        pg_path2_move_to(hp, hx0, hy0);
        pg_path2_line_to(hp, hx1, hy0);
        pg_path2_line_to(hp, hx1, yA);
        pg_path2_quadratic_curve_to(hp, hx1, hy1, o->x, hy1);
        pg_path2_quadratic_curve_to(hp, hx0, hy1, hx0, yA);
        pg_path2_line_to(hp, hx0, hy0);
        pg_path2_close(hp);
      } else {
        pg_path2_move_to(hp, hx0, hy0);
        pg_path2_line_to(hp, hx1, hy0);
        pg_path2_line_to(hp, hx1, hy1);
        pg_path2_line_to(hp, hx0, hy1);
        pg_path2_close(hp);
      }
    }
    PgMesh m;
    pg_mesh_begin(&m, ta.temp.arena, 256, 1024);
    pg_extrude_shape(&m, &shape, 1,
                     &(PgExtrudeShapeDesc){.depth = m_maxf(0.02f, s->t - bevel * 2.0f), .bevel_enabled = bevel > 0.0f,
                                           .bevel_thickness = bevel, .bevel_size = bevel, .bevel_offset = 0.0f, .bevel_segments = 1,
                                           .curve_segments = s->curve_segments ? s->curve_segments : 6, .steps = 1});
    g = cw_geo_from_pg(al, &m);
  }
  if (bevel > 0.0f)
    cw_geo_translate(&g, 0.0f, 0.0f, bevel);
  cw_geo_normals(&g);
  cw_geo_paint(&g, cw_panel_paint, NULL);
  if (s->warp > 0.0f) {
    for (u32 i = 0; i < g.nv; i++) {
      if (m_fabsf(g.nrm[i].z) < 0.5f)
        continue;
      f32 d = (cw_fbm3(g.pos[i].x * 0.5f + 3.7f, g.pos[i].y * 0.42f + 1.3f, 0.5, 2) - 0.5f) * s->warp * 2.0f;
      g.pos[i].z += d;
    }
    cw_geo_normals(&g);
  }
  return g;
}

// a simple polygon [x, z] extruded along +Y (an extrusion along +Z rotated -pi/2 about X)
hz_internal CwGeo cw_poly_prism(Allocator *al, const v2 *pts, u32 n, f32 height) {
  CwGeo g;
  ArenaAllocator *conflict = allocator_as_arena(al);
  tctx_temp_allocator_scope(ta, conflict) {
    PgShape shape;
    pg_shape_begin(&shape, &ta.allocator);
    pg_path2_move_to(&shape.path, pts[0].x, pts[0].y);
    for (u32 i = 1; i < n; i++)
      pg_path2_line_to(&shape.path, pts[i].x, pts[i].y);
    pg_path2_close(&shape.path);
    PgMesh m;
    pg_mesh_begin(&m, ta.temp.arena, 64, 256);
    pg_extrude_shape(&m, &shape, 1, &(PgExtrudeShapeDesc){.depth = height, .bevel_enabled = false, .bevel_segments = 1, .steps = 1});
    g = cw_geo_from_pg(al, &m);
  }
  cw_geo_apply(&g, cw_trs(0, 0, 0, 0, 1, 1, 1, -CW_PI_F * 0.5f, 0));
  cw_geo_normals(&g);
  return g;
}

// an irregular crumbled patch exposing the substrate, rotated to face +Z
hz_internal CwGeo cw_spall(Allocator *al, f32 w, f32 h, f32 depth);

hz_internal void cw_spall_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)ctx;
  (void)p;
  out[0] = 0.3f;
  out[1] = 0.55f + 0.3f * (1.0f - m_fabsf(n.z));
  out[2] = 0.5f * (1.0f - m_fabsf(n.z)) + 0.2f;
}

hz_internal CwGeo cw_spall(Allocator *al, f32 w, f32 h, f32 depth) {
  v2 pts[11];
  for (u32 i = 0; i < 11; i++) {
    f32 t = ((f32)i / 11.0f) * (f32)CW_TAU;
    f32 rr = 0.5f * (1.0f + (cw_fbm3(m_cosf(t) * 2.0f + 9.0f, m_sinf(t) * 2.0f + 3.0f, 1.7, 2) - 0.5f) * 0.9f);
    pts[i] = (v2){m_cosf(t) * rr * w, m_sinf(t) * rr * h};
  }
  CwGeo g = cw_poly_prism(al, pts, 11, depth);
  cw_geo_apply(&g, cw_trs(0, 0, 0, 0, 1, 1, 1, CW_PI_F * 0.5f, 0));
  cw_geo_paint(&g, cw_spall_paint, NULL);
  return g;
}
