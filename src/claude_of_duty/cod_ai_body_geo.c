// Claude of Duty soldier bodies: the loft toolkit every part is made of. u,v are metres of surface; the builder divides by the tile later.

// ------------------------------------------------------------------ noise ---

void cab_noise_init(CabNoise *n, CodRng *rng, Allocator *alloc) {
  u8 *p = ALLOC_ARRAY(alloc, u8, 512);
  for (u32 i = 0; i < 256; i++)
    p[i] = (u8)i;
  for (u32 i = 255; i > 0; i--) {
    u32 j = (u32)cod_rng_int(rng, 0, (i32)i);
    u8 t = p[i];
    p[i] = p[j];
    p[j] = t;
  }
  for (u32 i = 0; i < 256; i++)
    p[256 + i] = p[i];
  n->perm = p;
}

hz_internal const i8 cab_g3[12][3] = {{1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {1, 0, 1}, {-1, 0, 1},
                                      {1, 0, -1}, {-1, 0, -1}, {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1}};

hz_internal f32 cab_grad(u32 h, f32 dx, f32 dy, f32 dz) {
  const i8 *q = cab_g3[h % 12u];
  return (f32)q[0] * dx + (f32)q[1] * dy + (f32)q[2] * dz;
}

hz_internal f32 cab_fade(f32 t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

// Perlin 3D, roughly [-1,1]
f32 cab_n3(const CabNoise *n, f32 x, f32 y, f32 z) {
  const u8 *p = n->perm;
  f32 fx = m_floorf(x), fy = m_floorf(y), fz = m_floorf(z);
  u32 X = (u32)((i32)fx) & 255u, Y = (u32)((i32)fy) & 255u, Z = (u32)((i32)fz) & 255u;
  x -= fx;
  y -= fy;
  z -= fz;
  f32 u = cab_fade(x), v = cab_fade(y), w = cab_fade(z);
  u32 A = p[X] + Y, B = p[X + 1] + Y;
  u32 AA = p[A] + Z, AB = p[A + 1] + Z, BA = p[B] + Z, BB = p[B + 1] + Z;
  f32 l00 = m_lerpf(cab_grad(p[AA], x, y, z), cab_grad(p[BA], x - 1.0f, y, z), u);
  f32 l10 = m_lerpf(cab_grad(p[AB], x, y - 1.0f, z), cab_grad(p[BB], x - 1.0f, y - 1.0f, z), u);
  f32 l01 = m_lerpf(cab_grad(p[AA + 1], x, y, z - 1.0f), cab_grad(p[BA + 1], x - 1.0f, y, z - 1.0f), u);
  f32 l11 = m_lerpf(cab_grad(p[AB + 1], x, y - 1.0f, z - 1.0f), cab_grad(p[BB + 1], x - 1.0f, y - 1.0f, z - 1.0f), u);
  return m_lerpf(m_lerpf(l00, l10, v), m_lerpf(l01, l11, v), w);
}

f32 cab_fbm3(const CabNoise *n, f32 x, f32 y, f32 z, u32 oct) {
  f32 a = 0.5f, f = 1.0f, s = 0.0f, norm = 0.0f;
  for (u32 i = 0; i < oct; i++) {
    s += a * cab_n3(n, x * f, y * f, z * f);
    norm += a;
    a *= 0.5f;
    f *= 2.03f;
  }
  return s / norm;
}

// ----------------------------------------------------------------- meshes ---

PgMesh *cab_mesh(ArenaAllocator *arena) {
  PgMesh *m = ARENA_ALLOC(arena, PgMesh);
  pg_mesh_begin(m, arena, 256, 768);
  return m;
}

// n 2 = ellipse, 6+ = rounded box
void cab_super_ellipse(v2 *out, f32 rx, f32 rz, f32 n, u32 seg) {
  f32 e = 2.0f / n;
  for (u32 i = 0; i < seg; i++) {
    f32 t = (f32)i / (f32)seg * (2.0f * (f32)M_PI);
    f32 c = m_cosf(t), s = m_sinf(t);
    f32 sc = c > 0.0f ? 1.0f : c < 0.0f ? -1.0f : 0.0f;
    f32 ss = s > 0.0f ? 1.0f : s < 0.0f ? -1.0f : 0.0f;
    out[i] = (v2){rx * sc * m_powf(m_fabsf(c), e), rz * ss * m_powf(m_fabsf(s), e)};
  }
}

void cab_ellipse_profile(v2 *out, f32 rx, f32 rz, u32 seg) { cab_super_ellipse(out, rx, rz, 2.0f, seg); }

// loft: rings of k points into a tube; the seam column is duplicated for correct UVs
void cab_loft(PgMesh *out, const CabRing *rings, u32 ring_count, u32 k, b32 cap_start, b32 cap_end) {
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v3 *rp = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, ring_count * k);
  v3 *centre = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, ring_count);
  f32 *vlen = ALLOC_ARRAY_NO_ZERO(&ta.allocator, f32, ring_count);
  f32 *ua = ALLOC_ARRAY_NO_ZERO(&ta.allocator, f32, k + 1);
  u32 base = out->pos.len;

  f32 acc = 0.0f;
  for (u32 r = 0; r < ring_count; r++) {
    const CabRing *ring = &rings[r];
    v3 c = V3_ZERO;
    for (u32 j = 0; j < k; j++) {
      v3 v = quat_rotate(ring->q, (v3){ring->pts[j].x * ring->s.x, 0.0f, ring->pts[j].y * ring->s.y});
      v = v3_add(v, ring->o);
      rp[r * k + j] = v;
      c = v3_add(c, v);
    }
    c = v3_scale(c, 1.0f / (f32)k);
    centre[r] = c;
    if (r > 0)
      acc += v3_distance(c, centre[r - 1]);
    vlen[r] = acc;
  }
  ua[0] = 0.0f;
  for (u32 j = 1; j <= k; j++)
    ua[j] = ua[j - 1] + v3_distance(rp[j % k], rp[j - 1]);

  u32 cols = k + 1;
  for (u32 r = 0; r < ring_count; r++)
    for (u32 c = 0; c < cols; c++)
      pg_mesh_push_vertex(out, rp[r * k + (c % k)], V3_ZERO, (v2){ua[c], vlen[r]});
  for (u32 r = 0; r + 1 < ring_count; r++)
    for (u32 c = 0; c + 1 < cols; c++) {
      u32 a = base + r * cols + c;
      u32 b = a + 1;
      u32 d = base + (r + 1) * cols + c;
      u32 e = d + 1;
      pg_mesh_push_tri(out, a, d, b);
      pg_mesh_push_tri(out, b, d, e);
    }

  for (u32 pass = 0; pass < 2; pass++) {
    if ((pass == 0 && !cap_start) || (pass == 1 && !cap_end))
      continue;
    u32 ri = pass == 0 ? 0 : ring_count - 1;
    b32 flip = pass == 0;
    u32 c_idx = out->pos.len;
    pg_mesh_push_vertex(out, centre[ri], V3_ZERO, (v2){ua[k] * 0.5f, vlen[ri]});
    u32 start = out->pos.len;
    for (u32 j = 0; j < k; j++) {
      f32 ang = (f32)j / (f32)k * (2.0f * (f32)M_PI);
      pg_mesh_push_vertex(out, rp[ri * k + j], V3_ZERO, (v2){ua[k] * 0.5f + m_cosf(ang) * 0.02f, vlen[ri] + m_sinf(ang) * 0.02f});
    }
    for (u32 j = 0; j < k; j++) {
      u32 a = start + j;
      u32 b = start + ((j + 1) % k);
      if (flip)
        pg_mesh_push_tri(out, c_idx, b, a);
      else
        pg_mesh_push_tri(out, c_idx, a, b);
    }
  }
  tctx_temp_allocator_end(ta);
}

// ring frames along a path: +Y runs down the path, X is dir x up
void cab_path_frames(quat *out, const v3 *points, u32 n, v3 up_ref) {
  for (u32 i = 0; i < n; i++) {
    v3 a = points[i > 0 ? i - 1 : 0];
    v3 b = points[i + 1 < n ? i + 1 : n - 1];
    v3 dir = v3_sub(b, a);
    if (v3_lengthsq(dir) < 1e-12f)
      dir = (v3){0.0f, 1.0f, 0.0f};
    dir = v3_normalize(dir);
    v3 up = up_ref;
    if (m_fabsf(v3_dot(up, dir)) > 0.97f)
      up = (v3){1.0f, 0.0f, 0.0f};
    v3 x = v3_normalize(v3_cross(dir, up));
    v3 z = v3_normalize(v3_cross(x, dir));
    out[i] = quat_from_m3((m3){{{x.x, x.y, x.z}, {dir.x, dir.y, dir.z}, {z.x, z.y, z.z}}});
  }
}

// tube along a polyline; `profiles` holds n rings of k points
void cab_tube(PgMesh *out, const v3 *points, u32 n, const v2 *profiles, u32 k, v3 up_ref, b32 cap_start, b32 cap_end) {
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  quat *frames = ALLOC_ARRAY_NO_ZERO(&ta.allocator, quat, n);
  CabRing *rings = ALLOC_ARRAY_NO_ZERO(&ta.allocator, CabRing, n);
  cab_path_frames(frames, points, n, up_ref);
  for (u32 i = 0; i < n; i++)
    rings[i] = (CabRing){.pts = profiles + i * k, .o = points[i], .q = frames[i], .s = {1.0f, 1.0f}};
  cab_loft(out, rings, n, k, cap_start, cap_end);
  tctx_temp_allocator_end(ta);
}

// flat strap/belt extruded along a polyline; `upright` runs the width along `up`
void cab_ribbon(PgMesh *out, const v3 *points, u32 n, f32 width, f32 thick, u32 seg, v3 up, b32 upright) {
  f32 half = width * 0.5f, ht = thick * 0.5f;
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, n * seg);
  if (upright)
    cab_super_ellipse(prof, ht, half, 4.0f, seg);
  else
    cab_super_ellipse(prof, half, ht, 4.0f, seg);
  for (u32 i = 1; i < n; i++)
    mem_cpy(prof + i * seg, prof, sizeof(v2) * seg);
  cab_tube(out, points, n, prof, seg, up, true, true);
  tctx_temp_allocator_end(ta);
}

// superellipsoid slab: n sets the plan corners, round_y how much the top and bottom edges tuck in
void cab_box_round(PgMesh *out, f32 hx, f32 hy, f32 hz, f32 n, u32 seg, u32 rows, f32 round_y) {
  const f32 ny = 5.0f;
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, rows * seg);
  CabRing *rings = ALLOC_ARRAY_NO_ZERO(&ta.allocator, CabRing, rows);
  for (u32 r = 0; r < rows; r++) {
    f32 t = (f32)r / (f32)(rows - 1);
    f32 y = (t * 2.0f - 1.0f) * hy;
    f32 a = m_minf(1.0f, m_fabsf(y) / hy);
    f32 k = m_clampf((a - (1.0f - round_y)) / round_y, 0.0f, 1.0f);
    f32 env = m_powf(m_maxf(0.0f, 1.0f - m_powf(k, ny)), 1.0f / ny);
    f32 e = m_maxf(0.02f, env);
    cab_super_ellipse(prof + r * seg, hx * e, hz * e, n, seg);
    rings[r] = (CabRing){.pts = prof + r * seg, .o = {0.0f, y, 0.0f}, .q = quat_identity(), .s = {1.0f, 1.0f}};
  }
  cab_loft(out, rings, rows, seg, false, false);
  tctx_temp_allocator_end(ta);
}

// ellipsoid with a latitude clamp: v0 = 0 is the south pole
void cab_ellipsoid(PgMesh *out, f32 rx, f32 ry, f32 rz, u32 seg, u32 rows, f32 v0, f32 v1) {
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, rows * seg);
  CabRing *rings = ALLOC_ARRAY_NO_ZERO(&ta.allocator, CabRing, rows);
  for (u32 r = 0; r < rows; r++) {
    f32 t = v0 + (v1 - v0) * ((f32)r / (f32)(rows - 1));
    f32 phi = t * (f32)M_PI;
    f32 y = -m_cosf(phi) * ry;
    f32 s = m_sinf(phi);
    cab_ellipse_profile(prof + r * seg, m_maxf(1e-4f, rx * s), m_maxf(1e-4f, rz * s), seg);
    rings[r] = (CabRing){.pts = prof + r * seg, .o = {0.0f, y, 0.0f}, .q = quat_identity(), .s = {1.0f, 1.0f}};
  }
  cab_loft(out, rings, rows, seg, false, false);
  tctx_temp_allocator_end(ta);
}

void cab_compute_normals(PgMesh *m) { pg_mesh_smooth_normals(m, 0); }

// averages the normals of vertices within 1e-4 of each other so smooth shading crosses the loft seams
void cab_weld_normals(PgMesh *m) {
  u32 n = m->pos.len;
  if (n < 2)
    return;
  TempAllocator ta = tctx_temp_allocator_begin(m->arena);
  u32 cap = 16;
  while (cap < n * 2)
    cap <<= 1;
  i32 *head = ALLOC_ARRAY_NO_ZERO(&ta.allocator, i32, cap);
  for (u32 i = 0; i < cap; i++)
    head[i] = -1;
  i32 *next = ALLOC_ARRAY_NO_ZERO(&ta.allocator, i32, n);
  u32 *grp = ALLOC_ARRAY_NO_ZERO(&ta.allocator, u32, n);
  iv3 *key = ALLOC_ARRAY_NO_ZERO(&ta.allocator, iv3, n);
  for (u32 i = 0; i < n; i++) {
    v3 p = m->pos.items[i];
    iv3 k = {(i32)m_floorf(p.x * 1e4f + 0.5f), (i32)m_floorf(p.y * 1e4f + 0.5f), (i32)m_floorf(p.z * 1e4f + 0.5f)};
    key[i] = k;
    u32 h = ((u32)k.x * 73856093u ^ (u32)k.y * 19349663u ^ (u32)k.z * 83492791u) & (cap - 1u);
    i32 found = -1;
    for (i32 j = head[h]; j >= 0; j = next[j])
      if (key[j].x == k.x && key[j].y == k.y && key[j].z == k.z) {
        found = j;
        break;
      }
    if (found >= 0) {
      grp[i] = grp[found];
    } else {
      grp[i] = i;
      next[i] = head[h];
      head[h] = (i32)i;
    }
  }
  v3 *sum = ALLOC_ARRAY(&ta.allocator, v3, n);
  for (u32 i = 0; i < n; i++)
    sum[grp[i]] = v3_add(sum[grp[i]], m->nrm.items[i]);
  for (u32 i = 0; i < n; i++) {
    v3 s = sum[grp[i]];
    f32 len = v3_length(s);
    if (len > 1e-12f)
      m->nrm.items[i] = v3_scale(s, 1.0f / len);
  }
  tctx_temp_allocator_end(ta);
}

void cab_transform(PgMesh *m, m4 xform) { pg_mesh_transform(m, xform, 0); }

// normals first, then translate and rotate (Euler YXZ)
void cab_place(PgMesh *m, f32 x, f32 y, f32 z, f32 rx, f32 ry, f32 rz) {
  cab_compute_normals(m);
  quat q = quat_mul(quat_mul(quat_axis_angle((v3){0.0f, 1.0f, 0.0f}, ry), quat_axis_angle((v3){1.0f, 0.0f, 0.0f}, rx)),
                    quat_axis_angle((v3){0.0f, 0.0f, 1.0f}, rz));
  cab_transform(m, m4_trs((v3){x, y, z}, q, V3_ONE));
}

// cylindrical wrap about Y: bends a flat slab around the torso
void cab_bend_y(PgMesh *m, f32 radius, f32 centre_z) {
  for (u32 i = 0; i < m->pos.len; i++) {
    v3 v = m->pos.items[i];
    f32 r = radius + (v.z - centre_z);
    f32 a = v.x / radius;
    m->pos.items[i] = (v3){m_sinf(a) * r, v.y, centre_z + m_cosf(a) * r - radius};
  }
}
