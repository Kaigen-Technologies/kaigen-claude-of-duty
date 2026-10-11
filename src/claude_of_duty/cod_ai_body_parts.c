// Claude of Duty soldier bodies: the body, clothing and gear parts of the procedural soldier.

// moves vertex i along its normal by d
hz_internal void cab_push(PgMesh *m, u32 i, f32 d) {
  if (d != 0.0f)
    m->pos.items[i] = v3_add(m->pos.items[i], v3_scale(m->nrm.items[i], d));
}

// displaces every vertex along its normal by the expression; p and n name the vertex position and normal
#define CAB_DISPLACE(m, p, n, expr) \
  for (u32 _di = 0; _di < (m)->pos.len; _di++) { \
    v3 p = (m)->pos.items[_di]; \
    v3 n = (m)->nrm.items[_di]; \
    (void)n; \
    cab_push((m), _di, (expr)); \
  }

hz_internal const v3 CAB_UP_Y = {0.0f, 1.0f, 0.0f};
hz_internal const v3 CAB_UP_Z = {0.0f, 0.0f, 1.0f};
hz_internal const v3 CAB_UP_X = {1.0f, 0.0f, 0.0f};

hz_internal f32 cab_sq(f32 x) { return x * x; }

// ribbon, normals, append: the pattern every strap uses
hz_internal void cab_add_ribbon(PgMesh *out, const v3 *pts, u32 n, f32 width, f32 thick, u32 seg, v3 up, b32 upright) {
  PgMesh *r = cab_mesh(out->arena);
  cab_ribbon(r, pts, n, width, thick, seg, up, upright);
  cab_compute_normals(r);
  pg_mesh_append(out, r);
}

// rounded box, place, append
hz_internal void cab_add_box(PgMesh *out, f32 hx, f32 hy, f32 hz, f32 n, u32 seg, u32 rows, f32 round_y, v3 pos, v3 rot) {
  PgMesh *b = cab_mesh(out->arena);
  cab_box_round(b, hx, hy, hz, n, seg, rows, round_y);
  cab_place(b, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z);
  pg_mesh_append(out, b);
}

// the profile of ring i of n: ellipse (rx, rz) or superellipse, written to dst + i * k
hz_internal void cab_ring_profile(v2 *dst, u32 i, u32 k, f32 rx, f32 rz, f32 n) { cab_super_ellipse(dst + i * k, rx, rz, n, k); }

// ---------------------------------------------------------------- torso ---

hz_internal const f32 cab_jacket_S[13][5] = {{0.865f, 0.150f, 0.107f, -0.004f, 3.0f}, {0.925f, 0.156f, 0.110f, -0.008f, 3.0f},
                        {0.985f, 0.152f, 0.105f, -0.012f, 3.1f}, {1.055f, 0.146f, 0.100f, -0.014f, 3.2f},
                        {1.120f, 0.150f, 0.104f, -0.010f, 3.2f}, {1.185f, 0.161f, 0.112f, -0.004f, 3.1f},
                        {1.250f, 0.172f, 0.113f, 0.002f, 3.0f},  {1.310f, 0.184f, 0.117f, 0.005f, 2.9f},
                        {1.365f, 0.195f, 0.118f, 0.004f, 2.8f},  {1.418f, 0.198f, 0.111f, -0.002f, 2.7f},
                        {1.452f, 0.152f, 0.096f, -0.008f, 2.6f}, {1.482f, 0.098f, 0.080f, -0.010f, 2.4f},
                        {1.505f, 0.070f, 0.066f, -0.010f, 2.3f}};
// the jacket shell: lofted horizontal sections with a spinal curve and a deeper chest than back
hz_internal void cab_jacket(const CabNoise *nz, PgMesh *out, f32 bulk) {
  const u32 seg = 26, rows = 13;
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, rows * seg);
  CabRing *rings = ALLOC_ARRAY_NO_ZERO(&ta.allocator, CabRing, rows);
  for (u32 r = 0; r < rows; r++) {
    f32 k = r >= 6 && r <= 8 ? bulk : 1.0f;
    cab_super_ellipse(prof + r * seg, cab_jacket_S[r][1] * k, cab_jacket_S[r][2] * k, cab_jacket_S[r][4], seg);
    rings[r] = (CabRing){.pts = prof + r * seg, .o = {0.0f, cab_jacket_S[r][0], cab_jacket_S[r][3]}, .q = quat_identity(), .s = {1.0f, 1.0f}};
  }
  cab_loft(out, rings, rows, seg, true, false);
  tctx_temp_allocator_end(ta);
  cab_compute_normals(out);
  for (u32 i = 0; i < out->pos.len; i++) {
    v3 v = out->pos.items[i];
    f32 t = m_clampf((v.y - 1.1f) / 0.3f, 0.0f, 1.0f);
    if (v.z > 0.0f)
      v.z += 0.016f * t;
    else
      v.z -= 0.006f * t;
    if (v.y > 1.40f)
      v.y -= 0.02f * cab_sq(m_minf(1.0f, m_fabsf(v.x) / 0.18f));
    out->pos.items[i] = v;
  }
  cab_compute_normals(out);
  for (u32 _i = 0; _i < out->pos.len; _i++) {
    v3 p = out->pos.items[_i];
    f32 fold = cab_fbm3(nz, p.x * 22.0f, p.y * 15.0f, p.z * 22.0f, 3);
    f32 crease = m_sinf(p.y * 38.0f + fold * 3.4f) * 0.5f + 0.5f;
    f32 waist = m_expf(-cab_sq(p.y - 1.06f) / 0.006f);
    f32 gather = m_expf(-cab_sq(p.y - 0.93f) / 0.004f);
    cab_push(out, _i, fold * 0.0026f + crease * (waist * 0.0022f + gather * 0.0018f) + cab_fbm3(nz, p.x * 46.0f, p.y * 46.0f, p.z * 46.0f, 2) * 0.0007f);
  }
}

hz_internal void cab_rings_of(PgMesh *out, const f32 (*S)[3], u32 rows, u32 seg, f32 n, v3 off, b32 cap_start, b32 cap_end) {
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, rows * seg);
  CabRing *rings = ALLOC_ARRAY_NO_ZERO(&ta.allocator, CabRing, rows);
  for (u32 r = 0; r < rows; r++) {
    cab_super_ellipse(prof + r * seg, S[r][1], S[r][2], n, seg);
    rings[r] = (CabRing){.pts = prof + r * seg, .o = {off.x, S[r][0] + off.y, off.z}, .q = quat_identity(), .s = {1.0f, 1.0f}};
  }
  cab_loft(out, rings, rows, seg, cap_start, cap_end);
  tctx_temp_allocator_end(ta);
}

hz_internal const f32 cab_pelvis_S[5][3] = {{0.845f, 0.140f, 0.100f}, {0.885f, 0.148f, 0.106f}, {0.935f, 0.152f, 0.108f}, {0.985f, 0.150f, 0.104f}, {1.030f, 0.144f, 0.098f}};
// pelvis / seat block so the hips read solid between the jacket hem and the trousers
hz_internal void cab_pelvis(const CabNoise *nz, PgMesh *out) {
  cab_rings_of(out, cab_pelvis_S, 5, 22, 3.0f, (v3){0.0f, 0.0f, -0.006f}, true, true);
  cab_compute_normals(out);
  CAB_DISPLACE(out, p, n, cab_fbm3(nz, p.x * 26.0f, p.y * 20.0f, p.z * 26.0f, 3) * 0.004f);
}

hz_internal const f32 cab_collar_S[4][3] = {{1.435f, 0.108f, 0.092f}, {1.470f, 0.090f, 0.082f}, {1.500f, 0.082f, 0.076f}, {1.516f, 0.086f, 0.080f}};
// a short stand-up band around the neck
hz_internal void cab_collar(const CabNoise *nz, PgMesh *out) {
  cab_rings_of(out, cab_collar_S, 4, 22, 2.6f, (v3){0.0f, 0.0f, -0.006f}, false, true);
  cab_compute_normals(out);
  CAB_DISPLACE(out, p, n, cab_fbm3(nz, p.x * 40.0f, p.y * 30.0f, p.z * 40.0f, 2) * 0.003f);
}

// -------------------------------------------------------------- limbs ---

hz_internal f32 cab_radius_at(const f32 *radii, u32 count, f32 t) {
  u32 n = count - 1;
  f32 s = t * (f32)n;
  u32 i = (u32)m_minf((f32)(n - 1), m_floorf(s));
  f32 f = s - (f32)i;
  return radii[i] + (radii[i + 1] - radii[i]) * f;
}

// a tapered tube down a three-point chain with an elliptical section and cloth folds that follow arc length
hz_internal void cab_limb_tube(const CabNoise *nz, PgMesh *out, v3 a, v3 b, v3 c, const f32 *radii, u32 radius_count, u32 rings, u32 segs, f32 amp,
                               f32 crease, v3 bend) {
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v3 *pts = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, rings);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, rings * segs);
  for (u32 i = 0; i < rings; i++) {
    f32 t = (f32)i / (f32)(rings - 1);
    v3 tmp = t <= 0.5f ? v3_lerp(a, b, t * 2.0f) : v3_lerp(b, c, (t - 0.5f) * 2.0f);
    if (t > 0.34f && t < 0.66f) {
      f32 k = 1.0f - m_fabsf(t - 0.5f) / 0.16f;
      tmp = v3_lerp(tmp, v3_scale(v3_add(a, c), 0.5f), 0.06f * k);
    }
    pts[i] = tmp;
    f32 r = cab_radius_at(radii, radius_count, t);
    cab_ellipse_profile(prof + i * segs, r, r * 0.88f, segs);
  }
  cab_tube(out, pts, rings, prof, segs, CAB_UP_Z, false, false);
  tctx_temp_allocator_end(ta);
  cab_compute_normals(out);
  if (crease > 0.0f) {
    v3 ab = v3_sub(b, a), bc = v3_sub(c, b);
    f32 l_ab = v3_length(ab), l_bc = v3_length(bc);
    v3 u_ab = v3_scale(ab, 1.0f / m_maxf(1e-5f, l_ab));
    v3 u_bc = v3_scale(bc, 1.0f / m_maxf(1e-5f, l_bc));
    f32 total = l_ab + l_bc;
    v3 bn = v3_normalize(bend);
    for (u32 _i = 0; _i < out->pos.len; _i++) {
      v3 p = out->pos.items[_i];
      v3 n = out->nrm.items[_i];
      f32 t_ab = m_clampf(v3_dot(v3_sub(p, a), u_ab), 0.0f, l_ab);
      f32 t_bc = m_clampf(v3_dot(v3_sub(p, b), u_bc), 0.0f, l_bc);
      f32 s = t_ab < l_ab - 1e-4f ? t_ab : l_ab + t_bc;
      f32 u = s / total;
      f32 jit = cab_fbm3(nz, p.x * 6.0f, p.y * 5.0f, p.z * 6.0f, 2) - 0.5f;
      f32 band = m_fabsf(m_sinf((s / 0.055f + jit * 0.9f) * (f32)M_PI));
      f32 ridged = 1.0f - m_powf(band, 0.65f);
      f32 joint = m_expf(-cab_sq(u - 0.5f) / 0.012f);
      f32 cuff = m_expf(-cab_sq(u - 0.94f) / 0.004f);
      f32 inner = m_maxf(0.0f, v3_dot(bn, n));
      f32 gather = 1.0f + joint * (0.6f + 1.8f * inner) + cuff * 0.8f;
      f32 broad = cab_fbm3(nz, p.x * 9.0f, p.y * 7.0f + u * 3.1f, p.z * 9.0f, 3) - 0.5f;
      cab_push(out, _i, crease * (ridged * gather * 0.9f + broad * 1.1f));
    }
    cab_compute_normals(out);
  }
  for (u32 _i = 0; _i < out->pos.len; _i++) {
    v3 p = out->pos.items[_i];
    f32 f = cab_fbm3(nz, p.x * 11.0f, p.y * 9.0f, p.z * 11.0f, 3);
    f32 fine = cab_fbm3(nz, p.x * 34.0f, p.y * 30.0f, p.z * 34.0f, 2);
    cab_push(out, _i, f * amp + fine * amp * 0.3f);
  }
}

// deltoid cap so the shoulder is round rather than a tube end
hz_internal void cab_shoulder_cap(const CabNoise *nz, PgMesh *out, v3 sh, f32 side) {
  cab_ellipsoid(out, 0.052f, 0.064f, 0.056f, 18, 12, 0.0f, 1.0f);
  cab_compute_normals(out);
  for (u32 i = 0; i < out->pos.len; i++)
    if (out->pos.items[i].y < 0.0f)
      out->pos.items[i].x *= 0.9f;
  cab_place(out, sh.x + side * 0.012f, sh.y - 0.008f, sh.z, 0.0f, 0.0f, -side * 0.12f);
  CAB_DISPLACE(out, p, n, cab_fbm3(nz, p.x * 30.0f, p.y * 30.0f, p.z * 30.0f, 3) * 0.004f);
}

// ---------------------------------------------------------------- head ---

hz_internal const f32 cab_head_S[11][5] = {{0.000f, 0.038f, 0.050f, 0.020f, 2.6f},  {0.020f, 0.056f, 0.068f, 0.014f, 2.6f},  {0.044f, 0.068f, 0.076f, 0.007f, 2.5f},
                        {0.070f, 0.077f, 0.083f, 0.001f, 2.4f},  {0.095f, 0.084f, 0.088f, -0.002f, 2.4f}, {0.119f, 0.086f, 0.090f, -0.005f, 2.4f},
                        {0.146f, 0.083f, 0.089f, -0.009f, 2.4f}, {0.176f, 0.076f, 0.082f, -0.012f, 2.4f}, {0.205f, 0.062f, 0.066f, -0.014f, 2.4f},
                        {0.230f, 0.038f, 0.041f, -0.014f, 2.4f}, {0.244f, 0.012f, 0.013f, -0.014f, 2.4f}};
// skull and jaw lofted from anatomical sections; `base` is the Head bone
hz_internal void cab_head(const CabNoise *nz, PgMesh *out, v3 base) {
  const u32 seg = 24, rows = 11;
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, rows * seg);
  CabRing *rings = ALLOC_ARRAY_NO_ZERO(&ta.allocator, CabRing, rows);
  for (u32 r = 0; r < rows; r++) {
    cab_super_ellipse(prof + r * seg, cab_head_S[r][1], cab_head_S[r][2], cab_head_S[r][4], seg);
    rings[r] = (CabRing){.pts = prof + r * seg, .o = {base.x, base.y + cab_head_S[r][0], base.z + cab_head_S[r][3]}, .q = quat_identity(), .s = {1.0f, 1.0f}};
  }
  cab_loft(out, rings, rows, seg, true, false);
  tctx_temp_allocator_end(ta);
  cab_compute_normals(out);
  for (u32 i = 0; i < out->pos.len; i++) {
    v3 v = out->pos.items[i];
    f32 x = v.x - base.x, y = v.y - base.y, z = v.z - base.z;
    f32 front = m_maxf(0.0f, z / 0.09f);
    f32 brow = m_expf(-cab_sq(y - 0.113f) / 0.00016f) * front * m_expf(-(x * x) / 0.006f);
    f32 socket = m_expf(-cab_sq(m_fabsf(x) - 0.033f) / 0.00035f) * m_expf(-cab_sq(y - 0.098f) / 0.00022f) * front;
    f32 cheek = m_expf(-cab_sq(m_fabsf(x) - 0.055f) / 0.0009f) * m_expf(-cab_sq(y - 0.070f) / 0.0007f) * m_maxf(0.0f, z / 0.06f);
    f32 temple = m_expf(-cab_sq(y - 0.150f) / 0.0016f) * m_expf(-cab_sq(m_fabsf(x) - 0.082f) / 0.0006f);
    f32 chin = m_expf(-(y * y) / 0.00035f) * front;
    f32 occ = m_expf(-cab_sq(y - 0.165f) / 0.0018f) * m_maxf(0.0f, -z / 0.09f);
    f32 scale = 1.0f + 0.05f * brow - 0.10f * socket + 0.05f * cheek - 0.06f * temple;
    out->pos.items[i] = (v3){base.x + x * (1.0f - 0.05f * socket - 0.05f * temple), base.y + y,
                             base.z + z * scale + 0.006f * brow + 0.004f * chin + 0.008f * occ * -1.0f};
  }
  cab_compute_normals(out);
  CAB_DISPLACE(out, p, n, cab_fbm3(nz, p.x * 70.0f, p.y * 70.0f, p.z * 70.0f, 3) * 0.0012f);
}

hz_internal const f32 cab_nose_S[6][4] = {{0.118f, 0.075f, 0.009f, 0.010f}, {0.104f, 0.084f, 0.011f, 0.016f}, {0.088f, 0.093f, 0.014f, 0.020f},
                       {0.074f, 0.100f, 0.017f, 0.021f}, {0.064f, 0.100f, 0.020f, 0.018f}, {0.058f, 0.092f, 0.019f, 0.012f}};
// nose wedge and nostrils
hz_internal void cab_nose(PgMesh *out, v3 base) {
  const u32 seg = 12, rows = 6;
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, rows * seg);
  CabRing *rings = ALLOC_ARRAY_NO_ZERO(&ta.allocator, CabRing, rows);
  for (u32 r = 0; r < rows; r++) {
    cab_super_ellipse(prof + r * seg, cab_nose_S[r][2], cab_nose_S[r][3], 2.2f, seg);
    rings[r] = (CabRing){.pts = prof + r * seg, .o = {base.x, base.y + cab_nose_S[r][0], base.z + cab_nose_S[r][1]}, .q = quat_identity(), .s = {1.0f, 1.0f}};
  }
  cab_loft(out, rings, rows, seg, false, true);
  tctx_temp_allocator_end(ta);
  cab_compute_normals(out);
}

// ear: a folded flattened ellipsoid
hz_internal void cab_ear(PgMesh *out, v3 base, f32 side) {
  cab_ellipsoid(out, 0.010f, 0.030f, 0.020f, 12, 9, 0.0f, 1.0f);
  cab_compute_normals(out);
  for (u32 i = 0; i < out->pos.len; i++) {
    v3 v = out->pos.items[i];
    v.z += v.y * 0.25f;
    v.x += m_fabsf(v.y) * 0.10f;
    out->pos.items[i] = v;
  }
  cab_place(out, base.x + side * 0.083f, base.y + 0.098f, base.z - 0.008f, 0.1f, side * 0.25f, 0.0f);
}

// a small dark glossy sphere set into the socket
hz_internal void cab_eyeball(PgMesh *out, v3 base, f32 side) {
  cab_ellipsoid(out, 0.0125f, 0.0125f, 0.0125f, 12, 8, 0.0f, 1.0f);
  cab_compute_normals(out);
  cab_place(out, base.x + side * 0.032f, base.y + 0.0975f, base.z + 0.0665f, 0.0f, 0.0f, 0.0f);
}

hz_internal const f32 cab_neck_radii[3] = {0.058f, 0.056f, 0.054f};
// the neck is a limb tube with the head bone at its end
hz_internal void cab_neck(const CabNoise *nz, PgMesh *out, v3 head) {
  cab_limb_tube(nz, out, (v3){head.x, head.y - 0.10f, head.z - 0.012f}, (v3){head.x, head.y - 0.05f, head.z - 0.008f}, head, cab_neck_radii, 3, 5, 14, 0.001f,
                0.0f, (v3){0.0f, 0.0f, -1.0f});
}

hz_internal const f32 cab_face_wrap_S[8][5] = {{-0.075f, 0.062f, 0.062f, -0.010f, 2.6f}, {-0.040f, 0.070f, 0.072f, -0.006f, 2.6f}, {-0.010f, 0.080f, 0.084f, 0.004f, 2.5f},
                       {0.014f, 0.070f, 0.082f, 0.014f, 2.5f},   {0.038f, 0.078f, 0.086f, 0.008f, 2.5f},   {0.060f, 0.086f, 0.092f, 0.002f, 2.4f},
                       {0.076f, 0.090f, 0.094f, -0.002f, 2.4f},  {0.086f, 0.090f, 0.093f, -0.006f, 2.4f}};
// balaclava / shemagh over the lower face: a dome, a rolled hem along the eye line, a centre seam and the bridge fold
hz_internal void cab_face_wrap(const CabNoise *nz, PgMesh *out, v3 base) {
  const u32 seg = 22, rows = 8;
  PgMesh *m = cab_mesh(out->arena);
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, rows * seg);
  CabRing *rings = ALLOC_ARRAY_NO_ZERO(&ta.allocator, CabRing, rows);
  for (u32 r = 0; r < rows; r++) {
    cab_super_ellipse(prof + r * seg, cab_face_wrap_S[r][1], cab_face_wrap_S[r][2], cab_face_wrap_S[r][4], seg);
    rings[r] = (CabRing){.pts = prof + r * seg, .o = {base.x, base.y + cab_face_wrap_S[r][0], base.z + cab_face_wrap_S[r][3]}, .q = quat_identity(), .s = {1.0f, 1.0f}};
  }
  cab_loft(m, rings, rows, seg, false, false);
  cab_compute_normals(m);
  for (u32 _i = 0; _i < m->pos.len; _i++) {
    v3 p = m->pos.items[_i];
    f32 fold = cab_fbm3(nz, p.x * 30.0f, p.y * 24.0f, p.z * 30.0f, 3);
    f32 wrap = m_sinf(p.y * 90.0f + fold * 4.0f) * 0.5f + 0.5f;
    cab_push(m, _i, fold * 0.005f + wrap * 0.0035f);
  }
  pg_mesh_append(out, m);

  // rolled hem along the eye line: it rides higher over the cheeks and dips at the bridge
  const u32 n_hem = 26;
  v3 *hem = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, n_hem + 1);
  for (u32 i = 0; i <= n_hem; i++) {
    f32 a = (f32)i / (f32)n_hem * (2.0f * (f32)M_PI);
    f32 sx = m_sinf(a), sz = m_cosf(a);
    f32 y = 0.086f + m_maxf(0.0f, sz) * 0.006f - m_expf(-(sx * sx) / 0.06f) * m_maxf(0.0f, sz) * 0.010f;
    hem[i] = (v3){base.x + sx * 0.092f, base.y + y, base.z + sz * 0.096f - 0.004f};
  }
  cab_add_ribbon(out, hem, n_hem + 1, 0.015f, 0.008f, 6, CAB_UP_Y, true);
  // centre-front seam from the chin to the hem
  v3 *seam = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, 5);
  for (u32 i = 0; i <= 4; i++) {
    f32 t = (f32)i / 4.0f;
    seam[i] = (v3){base.x, base.y + 0.082f - t * 0.086f, base.z + 0.088f - t * 0.020f};
  }
  cab_add_ribbon(out, seam, 5, 0.009f, 0.004f, 5, CAB_UP_X, false);
  // bridge fold over the nose
  v3 *bridge = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, 3);
  bridge[0] = (v3){base.x - 0.042f, base.y + 0.070f, base.z + 0.070f};
  bridge[1] = (v3){base.x, base.y + 0.078f, base.z + 0.092f};
  bridge[2] = (v3){base.x + 0.042f, base.y + 0.070f, base.z + 0.070f};
  cab_add_ribbon(out, bridge, 3, 0.013f, 0.005f, 6, CAB_UP_Y, false);
  tctx_temp_allocator_end(ta);
  cab_compute_normals(out);
}

// wrap-around dark shooting glasses: a curved lens and two thin temples
hz_internal void cab_sunglasses(PgMesh *lens, PgMesh *frame, v3 base) {
  TempAllocator cab_tmp = tctx_temp_allocator_begin(NULL);
  cab_box_round(lens, 0.072f, 0.0155f, 0.006f, 3.0f, 18, 5, 0.6f);
  cab_place(lens, base.x, base.y + 0.100f, base.z + 0.080f, -0.06f, 0.0f, 0.0f);
  cab_bend_y(lens, 0.098f, 0.0f);
  cab_compute_normals(lens);
  for (i32 side = -1; side <= 1; side += 2) {
    f32 s = (f32)side;
    v3 *arm = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 3);
    arm[0] = (v3){base.x + s * 0.070f, base.y + 0.104f, base.z + 0.062f};
    arm[1] = (v3){base.x + s * 0.083f, base.y + 0.104f, base.z + 0.010f};
    arm[2] = (v3){base.x + s * 0.080f, base.y + 0.100f, base.z - 0.030f};
    cab_add_ribbon(frame, arm, 3, 0.008f, 0.004f, 5, CAB_UP_Y, true);
  }
  cab_compute_normals(frame);
  tctx_temp_allocator_end(cab_tmp);
}

// -------------------------------------------------------------- helmet ---

// high-cut ballistic helmet: a scalloped shell and a brim lip
hz_internal void cab_helmet(const CabNoise *nz, PgMesh *out, v3 base) {
  const f32 cy = base.y + 0.100f, rx = 0.121f, ry = 0.158f, rz = 0.135f;
  const u32 seg = 26, rows = 12;
  PgMesh *shell = cab_mesh(out->arena);
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, rows * seg);
  CabRing *rings = ALLOC_ARRAY_NO_ZERO(&ta.allocator, CabRing, rows);
  for (u32 r = 0; r < rows; r++) {
    f32 t = (f32)r / (f32)(rows - 1);
    f32 phi = (0.5f + 0.5f * t) * (f32)M_PI;
    f32 y = -m_cosf(phi) * ry;
    f32 s = m_sinf(phi);
    cab_ellipse_profile(prof + r * seg, rx * m_maxf(0.08f, s), rz * m_maxf(0.08f, s), seg);
    rings[r] = (CabRing){.pts = prof + r * seg, .o = {base.x, cy + y, base.z - 0.006f}, .q = quat_identity(), .s = {1.0f, 1.0f}};
  }
  cab_loft(shell, rings, rows, seg, false, false);
  cab_compute_normals(shell);
  // scallop: raise the rim over the ears, drop it at the front and back
  for (u32 i = 0; i < shell->pos.len; i++) {
    v3 v = shell->pos.items[i];
    f32 dy = v.y - cy;
    if (dy > 0.012f)
      continue;
    f32 ang = m_atan2f(v.x - base.x, v.z - base.z);
    f32 side = m_fabsf(m_sinf(ang));
    f32 lift = side * side * 0.042f - m_maxf(0.0f, m_cosf(ang)) * 0.010f;
    f32 k = m_clampf((0.012f - dy) / 0.06f, 0.0f, 1.0f);
    shell->pos.items[i].y = v.y + lift * k;
  }
  cab_compute_normals(shell);
  CAB_DISPLACE(shell, p, n, cab_fbm3(nz, p.x * 40.0f, p.y * 40.0f, p.z * 40.0f, 3) * 0.0016f);
  pg_mesh_append(out, shell);

  // brim lip: a thin band following the rim
  const u32 n_lip = 30;
  v3 *lip = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, n_lip + 1);
  for (u32 i = 0; i <= n_lip; i++) {
    f32 a = (f32)i / (f32)n_lip * (2.0f * (f32)M_PI);
    f32 sx = m_sinf(a), sz = m_cosf(a);
    f32 side = m_fabsf(sx);
    f32 lift = side * side * 0.042f - m_maxf(0.0f, sz) * 0.010f;
    lip[i] = (v3){base.x + sx * rx * 0.955f, cy + lift - 0.001f, base.z - 0.004f + sz * rz * 0.955f};
  }
  cab_add_ribbon(out, lip, n_lip + 1, 0.011f, 0.006f, 6, CAB_UP_Y, true);
  tctx_temp_allocator_end(ta);
}

// side rails, NVG shroud and the rear counterweight pouch
hz_internal void cab_helmet_hw(const CabNoise *nz, PgMesh *out, v3 base) {
  TempAllocator cab_tmp = tctx_temp_allocator_begin(NULL);
  const f32 cy = base.y + 0.100f;
  cab_add_box(out, 0.030f, 0.012f, 0.022f, 4.0f, 12, 5, 0.5f, (v3){base.x, cy + 0.062f, base.z + 0.120f}, (v3){-0.50f, 0.0f, 0.0f});
  cab_add_box(out, 0.009f, 0.016f, 0.007f, 4.0f, 8, 4, 0.4f, (v3){base.x, cy + 0.086f, base.z + 0.126f}, (v3){-0.50f, 0.0f, 0.0f});
  for (i32 side = -1; side <= 1; side += 2) {
    f32 s = (f32)side;
    v3 *pts = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 6);
    for (u32 i = 0; i <= 5; i++) {
      f32 t = (f32)i / 5.0f;
      f32 a = (-0.55f + t * 1.1f) * s;
      pts[i] = (v3){base.x + s * 0.114f * m_cosf(a * 0.6f), cy + 0.052f + m_sinf(t * (f32)M_PI) * 0.016f, base.z - 0.004f + m_sinf(a) * 0.118f};
    }
    cab_add_ribbon(out, pts, 6, 0.016f, 0.009f, 6, CAB_UP_Y, true);
  }
  PgMesh *cw = cab_mesh(out->arena);
  cab_box_round(cw, 0.058f, 0.034f, 0.026f, 4.0f, 14, 6, 0.5f);
  cab_place(cw, base.x, cy + 0.075f, base.z - 0.128f, 0.28f, 0.0f, 0.0f);
  cab_compute_normals(cw);
  CAB_DISPLACE(cw, p, n, cab_fbm3(nz, p.x * 40.0f, p.y * 40.0f, p.z * 40.0f, 2) * 0.002f);
  pg_mesh_append(out, cw);
  tctx_temp_allocator_end(cab_tmp);
}

// chin strap and nape pad
hz_internal void cab_chin_strap(PgMesh *out, v3 base) {
  TempAllocator cab_tmp = tctx_temp_allocator_begin(NULL);
  const f32 cy = base.y + 0.100f;
  for (i32 side = -1; side <= 1; side += 2) {
    f32 s = (f32)side;
    v3 *fwd = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 4);
    fwd[0] = (v3){base.x + s * 0.104f, cy + 0.004f, base.z + 0.036f};
    fwd[1] = (v3){base.x + s * 0.086f, cy - 0.058f, base.z + 0.056f};
    fwd[2] = (v3){base.x + s * 0.048f, cy - 0.104f, base.z + 0.062f};
    fwd[3] = (v3){base.x + s * 0.014f, cy - 0.118f, base.z + 0.054f};
    cab_add_ribbon(out, fwd, 4, 0.016f, 0.005f, 6, CAB_UP_Z, false);
    v3 *rear = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 3);
    rear[0] = (v3){base.x + s * 0.106f, cy, base.z - 0.024f};
    rear[1] = (v3){base.x + s * 0.090f, cy - 0.058f, base.z - 0.058f};
    rear[2] = (v3){base.x + s * 0.040f, cy - 0.078f, base.z - 0.082f};
    cab_add_ribbon(out, rear, 3, 0.014f, 0.005f, 5 + 1, CAB_UP_Y, false);
  }
  tctx_temp_allocator_end(cab_tmp);
}

// goggles frame: pushed up on the shell, or pulled down over the eyes
hz_internal void cab_goggle_frame(PgMesh *out, v3 base, b32 down) {
  if (down) {
    cab_box_round(out, 0.078f, 0.028f, 0.026f, 3.2f, 20, 6, 0.5f);
    cab_place(out, base.x, base.y + 0.098f, base.z + 0.072f, -0.10f, 0.0f, 0.0f);
    cab_bend_y(out, 0.115f, 0.0f);
  } else {
    cab_box_round(out, 0.082f, 0.026f, 0.024f, 3.2f, 20, 6, 0.5f);
    cab_place(out, base.x, base.y + 0.176f, base.z + 0.098f, -0.95f, 0.0f, 0.0f);
    cab_bend_y(out, 0.15f, 0.0f);
  }
  cab_compute_normals(out);
}

hz_internal void cab_goggle_strap(PgMesh *out, v3 base, b32 down) {
  TempAllocator cab_tmp = tctx_temp_allocator_begin(NULL);
  const f32 bx = base.x, by = base.y, bz = base.z;
  if (down) {
    v3 *pts = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 6);
    pts[0] = (v3){bx - 0.084f, by + 0.100f, bz + 0.058f};
    pts[1] = (v3){bx - 0.106f, by + 0.108f, bz - 0.030f};
    pts[2] = (v3){bx - 0.062f, by + 0.116f, bz - 0.108f};
    pts[3] = (v3){bx + 0.062f, by + 0.116f, bz - 0.108f};
    pts[4] = (v3){bx + 0.106f, by + 0.108f, bz - 0.030f};
    pts[5] = (v3){bx + 0.084f, by + 0.100f, bz + 0.058f};
    cab_ribbon(out, pts, 6, 0.026f, 0.008f, 6, CAB_UP_Y, true);
  } else {
    v3 *pts = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 6);
    pts[0] = (v3){bx - 0.098f, by + 0.176f, bz + 0.078f};
    pts[1] = (v3){bx - 0.118f, by + 0.198f, bz - 0.020f};
    pts[2] = (v3){bx - 0.072f, by + 0.226f, bz - 0.116f};
    pts[3] = (v3){bx + 0.072f, by + 0.226f, bz - 0.116f};
    pts[4] = (v3){bx + 0.118f, by + 0.198f, bz - 0.020f};
    pts[5] = (v3){bx + 0.098f, by + 0.176f, bz + 0.078f};
    cab_ribbon(out, pts, 6, 0.024f, 0.007f, 6, CAB_UP_Y, true);
  }
  cab_compute_normals(out);
  tctx_temp_allocator_end(cab_tmp);
}

// goggle lens: a curved slab of smoked glass
hz_internal void cab_goggle_lens(PgMesh *out, v3 base, b32 down) {
  if (down) {
    cab_box_round(out, 0.071f, 0.020f, 0.008f, 3.0f, 18, 5, 0.6f);
    cab_place(out, base.x, base.y + 0.098f, base.z + 0.090f, -0.10f, 0.0f, 0.0f);
    cab_bend_y(out, 0.105f, 0.0f);
  } else {
    cab_box_round(out, 0.074f, 0.019f, 0.008f, 3.0f, 18, 5, 0.6f);
    cab_place(out, base.x, base.y + 0.176f, base.z + 0.115f, -0.95f, 0.0f, 0.0f);
    cab_bend_y(out, 0.14f, 0.0f);
  }
  cab_compute_normals(out);
}

// skull-hugging wrap for the un-helmeted variant: dome, rolled brim and a tail down the back
hz_internal void cab_scarf(const CabNoise *nz, PgMesh *out, v3 base) {
  PgMesh *dome = cab_mesh(out->arena);
  cab_ellipsoid(dome, 0.102f, 0.146f, 0.112f, 22, 12, 0.34f, 1.0f);
  cab_compute_normals(dome);
  cab_place(dome, base.x, base.y + 0.104f, base.z - 0.008f, 0.0f, 0.0f, 0.0f);
  for (u32 _i = 0; _i < dome->pos.len; _i++) {
    v3 p = dome->pos.items[_i];
    f32 f = cab_fbm3(nz, p.x * 26.0f, p.y * 22.0f, p.z * 26.0f, 3);
    cab_push(dome, _i, f * 0.006f + m_sinf(p.y * 70.0f + f * 4.0f) * 0.0022f);
  }
  pg_mesh_append(out, dome);
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v3 *pts = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, 25);
  for (u32 i = 0; i <= 24; i++) {
    f32 a = (f32)i / 24.0f * (2.0f * (f32)M_PI);
    pts[i] = (v3){base.x + m_sinf(a) * 0.099f, base.y + 0.118f - m_maxf(0.0f, m_cosf(a)) * 0.012f, base.z - 0.008f + m_cosf(a) * 0.109f};
  }
  cab_add_ribbon(out, pts, 25, 0.030f, 0.016f, 7, CAB_UP_Y, true);
  v3 *tail = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, 6);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, 6 * 12);
  for (u32 i = 0; i <= 5; i++) {
    f32 t = (f32)i / 5.0f;
    tail[i] = (v3){base.x + 0.028f * t, base.y + 0.115f - t * 0.20f, base.z - 0.085f - m_sinf(t * 2.2f) * 0.03f};
    cab_super_ellipse(prof + i * 12, 0.052f - t * 0.012f, 0.020f + t * 0.006f, 3.0f, 12);
  }
  PgMesh *tl = cab_mesh(out->arena);
  cab_tube(tl, tail, 6, prof, 12, CAB_UP_Z, false, true);
  cab_compute_normals(tl);
  CAB_DISPLACE(tl, p, n, cab_fbm3(nz, p.x * 30.0f, p.y * 26.0f, p.z * 30.0f, 3) * 0.006f);
  pg_mesh_append(out, tl);
  tctx_temp_allocator_end(ta);
}

// ----------------------------------------------------- load-bearing gear ---

// one plate: a curved slab with a soft edge that narrows toward the waist
hz_internal void cab_plate(PgMesh *out, f32 hx, f32 hy, f32 hz, f32 y, f32 z, f32 tilt, f32 radius) {
  cab_box_round(out, hx, hy, hz, 3.6f, 22, 11, 0.24f);
  for (u32 i = 0; i < out->pos.len; i++) {
    v3 v = out->pos.items[i];
    f32 t = m_maxf(0.0f, -v.y / hy);
    v.x *= 1.0f - 0.20f * t * t;
    v.z *= 1.0f - 0.35f * t * t;
    out->pos.items[i] = v;
  }
  cab_compute_normals(out);
  cab_place(out, 0.0f, y, z, tilt, 0.0f, 0.0f);
  cab_bend_y(out, radius, z);
  cab_compute_normals(out);
}

// a pouch: rounded box with a lid, a pull tab and compression stitching
hz_internal void cab_pouch(const CabNoise *nz, PgMesh *out, const CabPouch *o) {
  TempAllocator cab_tmp = tctx_temp_allocator_begin(NULL);
  f32 hx = o->hx, hy = o->hy, hz = o->hz;
  PgMesh *body = cab_mesh(out->arena);
  cab_box_round(body, hx, hy, hz, 5.5f, 18, 8, 0.18f);
  cab_compute_normals(body);
  CAB_DISPLACE(body, p, n, cab_fbm3(nz, p.x * 40.0f, p.y * 40.0f, p.z * 40.0f, 3) * 0.0022f);
  pg_mesh_append(out, body);
  PgMesh *lid = cab_mesh(out->arena);
  cab_box_round(lid, hx * 1.03f, 0.010f, hz * 0.98f, 5.5f, 18, 4, 0.5f);
  cab_place(lid, 0.0f, hy - 0.004f, (o->lid_tilt != 0.0f ? hz * 0.35f : 0.0f) + hz * 0.10f, o->lid_tilt - 0.18f, 0.0f, 0.0f);
  cab_compute_normals(lid);
  pg_mesh_append(out, lid);
  v3 *tab = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 3);
  tab[0] = (v3){0.0f, hy + 0.004f, hz * 0.7f};
  tab[1] = (v3){0.0f, hy - 0.010f, hz * 1.16f};
  tab[2] = (v3){0.0f, hy - 0.034f, hz * 1.10f};
  cab_add_ribbon(out, tab, 3, 0.014f, 0.004f, 5, CAB_UP_X, false);
  cab_place(out, o->x, o->y, o->z, o->rx, o->ry, o->rz);
  if (o->bend != 0.0f)
    cab_bend_y(out, o->bend, o->z);
  cab_compute_normals(out);
  tctx_temp_allocator_end(cab_tmp);
}

hz_internal void cab_carrier(const CabNoise *nz, PgMesh *out) {
  PgMesh *front = cab_mesh(out->arena);
  cab_plate(front, 0.152f, 0.140f, 0.030f, 1.298f, 0.126f, -0.05f, 0.20f);
  CAB_DISPLACE(front, p, n, cab_fbm3(nz, p.x * 34.0f, p.y * 34.0f, p.z * 34.0f, 3) * 0.0026f);
  pg_mesh_append(out, front);
  PgMesh *back = cab_mesh(out->arena);
  cab_plate(back, 0.154f, 0.148f, 0.026f, 1.300f, -0.116f, 0.05f, 0.21f);
  CAB_DISPLACE(back, p, n, cab_fbm3(nz, p.x * 34.0f, p.y * 34.0f, p.z * 34.0f, 3) * 0.0026f);
  pg_mesh_append(out, back);

  // cummerbund wrapping the waist
  const u32 n = 26;
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v3 *cb = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, n + 1);
  for (u32 i = 0; i <= n; i++) {
    f32 a = (f32)i / (f32)n * (2.0f * (f32)M_PI);
    cb[i] = (v3){m_sinf(a) * 0.168f, 1.152f + m_cosf(a * 2.0f) * 0.005f, m_cosf(a) * 0.121f - 0.004f};
  }
  PgMesh *band = cab_mesh(out->arena);
  cab_ribbon(band, cb, n + 1, 0.100f, 0.022f, 8, CAB_UP_Y, true);
  cab_compute_normals(band);
  CAB_DISPLACE(band, p, nn, cab_fbm3(nz, p.x * 34.0f, p.y * 34.0f, p.z * 34.0f, 3) * 0.002f);
  pg_mesh_append(out, band);
  tctx_temp_allocator_end(ta);

  for (i32 side = -1; side <= 1; side += 2) {
    f32 s = (f32)side;
    v3 *pts = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, 4);
    pts[0] = (v3){s * 0.082f, 1.418f, 0.144f};
    pts[1] = (v3){s * 0.100f, 1.468f, 0.040f};
    pts[2] = (v3){s * 0.104f, 1.462f, -0.036f};
    pts[3] = (v3){s * 0.092f, 1.418f, -0.120f};
    PgMesh *strap = cab_mesh(out->arena);
    cab_ribbon(strap, pts, 4, 0.076f, 0.030f, 8, CAB_UP_Y, false);
    cab_compute_normals(strap);
    CAB_DISPLACE(strap, p, nn, cab_fbm3(nz, p.x * 34.0f, p.y * 34.0f, p.z * 34.0f, 3) * 0.002f);
    pg_mesh_append(out, strap);
  }
}

hz_internal const v3 cab_webbing_drag[4] = {{-0.052f, 1.432f, -0.132f}, {-0.022f, 1.458f, -0.152f}, {0.022f, 1.458f, -0.152f}, {0.052f, 1.432f, -0.132f}};
// PALS rows across the front plate and the drag handle
hz_internal void cab_webbing(PgMesh *out) {
  TempAllocator cab_tmp = tctx_temp_allocator_begin(NULL);
  for (u32 r = 0; r < 2; r++) {
    f32 y = 1.322f + (f32)r * 0.046f;
    v3 *pts = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 9);
    for (u32 i = 0; i <= 8; i++) {
      f32 t = (f32)i / 8.0f;
      f32 x = (t - 0.5f) * 0.150f;
      pts[i] = (v3){x, y, 0.150f - (x * x) / 0.20f};
    }
    cab_add_ribbon(out, pts, 9, 0.013f, 0.0035f, 5, CAB_UP_Y, true);
  }
  cab_add_ribbon(out, cab_webbing_drag, 4, 0.028f, 0.010f, 6, CAB_UP_Y, true);
  tctx_temp_allocator_end(cab_tmp);
}

// two-point sling routed across the chest
hz_internal void cab_sling(PgMesh *out, v3 grip, v3 stock) {
  TempAllocator cab_tmp = tctx_temp_allocator_begin(NULL);
  v3 *pts = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 8);
  pts[0] = (v3){stock.x, stock.y + 0.02f, stock.z};
  pts[1] = (v3){-0.130f, 1.395f, -0.010f};
  pts[2] = (v3){-0.120f, 1.430f, -0.090f};
  pts[3] = (v3){0.020f, 1.430f, -0.118f};
  pts[4] = (v3){0.120f, 1.330f, -0.070f};
  pts[5] = (v3){0.150f, 1.250f, 0.040f};
  pts[6] = (v3){0.110f, 1.235f, 0.135f};
  pts[7] = (v3){grip.x + 0.02f, grip.y + 0.03f, grip.z + 0.02f};
  cab_ribbon(out, pts, 8, 0.032f, 0.009f, 6, CAB_UP_Y, false);
  cab_compute_normals(out);
  tctx_temp_allocator_end(cab_tmp);
}

hz_internal void cab_belt(const CabNoise *nz, PgMesh *out) {
  TempAllocator cab_tmp = tctx_temp_allocator_begin(NULL);
  const u32 n = 24;
  v3 *pts = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 25);
  for (u32 i = 0; i <= n; i++) {
    f32 a = (f32)i / (f32)n * (2.0f * (f32)M_PI);
    pts[i] = (v3){m_sinf(a) * 0.158f, 0.902f, m_cosf(a) * 0.113f - 0.008f};
  }
  PgMesh *b = cab_mesh(out->arena);
  cab_ribbon(b, pts, n + 1, 0.056f, 0.018f, 7, CAB_UP_Y, true);
  cab_compute_normals(b);
  CAB_DISPLACE(b, p, nn, cab_fbm3(nz, p.x * 40.0f, p.y * 40.0f, p.z * 40.0f, 2) * 0.0018f);
  pg_mesh_append(out, b);
  tctx_temp_allocator_end(cab_tmp);
}

hz_internal const f32 cab_knee_pad_dys[2] = {-0.056f, 0.052f};
// knee pad: a curved cap with two elastic straps
hz_internal void cab_knee_pad(const CabNoise *nz, PgMesh *out, v3 knee) {
  TempAllocator cab_tmp = tctx_temp_allocator_begin(NULL);
  PgMesh *cap = cab_mesh(out->arena);
  cab_box_round(cap, 0.064f, 0.080f, 0.026f, 4.5f, 18, 9, 0.42f);
  cab_place(cap, 0.0f, 0.0f, 0.052f, 0.0f, 0.0f, 0.0f);
  cab_bend_y(cap, 0.075f, 0.052f);
  cab_compute_normals(cap);
  CAB_DISPLACE(cap, p, n, cab_fbm3(nz, p.x * 60.0f, p.y * 60.0f, p.z * 60.0f, 3) * 0.0018f);
  pg_mesh_append(out, cap);
  for (u32 k = 0; k < 2; k++) {
    v3 *pts = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 15);
    for (u32 i = 0; i <= 14; i++) {
      f32 a = (f32)i / 14.0f * (2.0f * (f32)M_PI);
      pts[i] = (v3){m_sinf(a) * 0.066f, cab_knee_pad_dys[k], m_cosf(a) * 0.058f + 0.006f};
    }
    cab_add_ribbon(out, pts, 15, 0.016f, 0.006f, 6, CAB_UP_Y, true);
  }
  cab_place(out, knee.x, knee.y + 0.012f, knee.z + 0.004f, 0.06f, 0.0f, 0.0f);
  cab_compute_normals(out);
  tctx_temp_allocator_end(cab_tmp);
}

// ------------------------------------------------------- boots, gloves ---

hz_internal const f32 cab_boot_S[7][4] = {{-0.078f, 0.036f, 0.030f, 0.052f}, {-0.052f, 0.044f, 0.038f, 0.062f}, {-0.016f, 0.048f, 0.044f, 0.058f}, {0.030f, 0.049f, 0.046f, 0.048f},
                       {0.076f, 0.046f, 0.042f, 0.038f},  {0.112f, 0.040f, 0.034f, 0.030f},  {0.134f, 0.028f, 0.022f, 0.024f}};
// boot upper lofted front to back, and the ankle cuff up the shin
hz_internal void cab_boot(const CabNoise *nz, PgMesh *out, v3 ankle) {
  const u32 seg = 18, rows = 7;
  const quat qx = quat_axis_angle((v3){1.0f, 0.0f, 0.0f}, (f32)M_PI / 2.0f);
  PgMesh *upper = cab_mesh(out->arena);
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, rows * seg);
  CabRing *rings = ALLOC_ARRAY_NO_ZERO(&ta.allocator, CabRing, rows);
  for (u32 r = 0; r < rows; r++) {
    cab_super_ellipse(prof + r * seg, cab_boot_S[r][1], cab_boot_S[r][2], 2.8f, seg);
    rings[r] = (CabRing){.pts = prof + r * seg, .o = {ankle.x, ankle.y - 0.088f + cab_boot_S[r][3], ankle.z + cab_boot_S[r][0]}, .q = qx, .s = {1.0f, 1.0f}};
  }
  cab_loft(upper, rings, rows, seg, true, true);
  cab_compute_normals(upper);
  CAB_DISPLACE(upper, p, n, cab_fbm3(nz, p.x * 44.0f, p.y * 44.0f, p.z * 44.0f, 3) * 0.0022f);
  pg_mesh_append(out, upper);

  v3 *pts = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, 3);
  pts[0] = (v3){ankle.x, ankle.y + 0.010f, ankle.z - 0.004f};
  pts[1] = (v3){ankle.x, ankle.y + 0.070f, ankle.z - 0.002f};
  pts[2] = (v3){ankle.x, ankle.y + 0.125f, ankle.z + 0.002f};
  v2 *cp = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, 3 * 16);
  for (u32 i = 0; i < 3; i++) {
    f32 t = (f32)i / 2.0f;
    cab_ellipse_profile(cp + i * 16, 0.056f - 0.004f * t, 0.050f - 0.002f * t, 16);
  }
  PgMesh *cuff = cab_mesh(out->arena);
  cab_tube(cuff, pts, 3, cp, 16, CAB_UP_Z, false, false);
  cab_compute_normals(cuff);
  CAB_DISPLACE(cuff, p, n, cab_fbm3(nz, p.x * 44.0f, p.y * 44.0f, p.z * 44.0f, 3) * 0.0025f);
  pg_mesh_append(out, cuff);
  tctx_temp_allocator_end(ta);
}

hz_internal const f32 cab_boot_sole_S[7][3] = {{-0.082f, 0.033f, 0.018f}, {-0.055f, 0.043f, 0.020f}, {-0.020f, 0.047f, 0.014f}, {0.030f, 0.049f, 0.013f},
                       {0.080f, 0.046f, 0.013f},  {0.118f, 0.038f, 0.013f},  {0.140f, 0.024f, 0.012f}};
// boot sole and heel block, rubber
hz_internal void cab_boot_sole(PgMesh *out, v3 ankle) {
  const u32 seg = 16, rows = 7;
  const quat qx = quat_axis_angle((v3){1.0f, 0.0f, 0.0f}, (f32)M_PI / 2.0f);
  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, rows * seg);
  CabRing *rings = ALLOC_ARRAY_NO_ZERO(&ta.allocator, CabRing, rows);
  for (u32 r = 0; r < rows; r++) {
    cab_super_ellipse(prof + r * seg, cab_boot_sole_S[r][1], cab_boot_sole_S[r][2], 3.6f, seg);
    rings[r] = (CabRing){.pts = prof + r * seg, .o = {ankle.x, ankle.y - 0.088f + cab_boot_sole_S[r][2] + 0.001f, ankle.z + cab_boot_sole_S[r][0]}, .q = qx, .s = {1.0f, 1.0f}};
  }
  cab_loft(out, rings, rows, seg, true, true);
  tctx_temp_allocator_end(ta);
  cab_compute_normals(out);
  PgMesh *heel = cab_mesh(out->arena);
  cab_box_round(heel, 0.036f, 0.011f, 0.030f, 4.0f, 12, 4, 0.4f);
  cab_place(heel, ankle.x, ankle.y - 0.082f, ankle.z - 0.056f, 0.0f, 0.0f, 0.0f);
  pg_mesh_append(out, heel);
  cab_compute_normals(out);
}

// cross-over ribbons up the boot tongue
hz_internal void cab_boot_laces(PgMesh *out, v3 ankle) {
  TempAllocator cab_tmp = tctx_temp_allocator_begin(NULL);
  for (u32 i = 0; i < 5; i++) {
    f32 t = (f32)i / 4.0f;
    f32 z = ankle.z + 0.088f - t * 0.076f;
    f32 y = ankle.y - 0.028f + t * 0.070f;
    f32 w = 0.030f - t * 0.004f;
    v3 *pts = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 3);
    pts[0] = (v3){ankle.x - w, y - 0.006f, z + 0.006f};
    pts[1] = (v3){ankle.x, y + 0.004f, z};
    pts[2] = (v3){ankle.x + w, y - 0.006f, z + 0.006f};
    cab_add_ribbon(out, pts, 3, 0.008f, 0.004f, 5, CAB_UP_Y, false);
  }
  tctx_temp_allocator_end(cab_tmp);
}

// the hand frame S, A, N at `pos` as a column-major matrix
hz_internal m4 cab_basis(v3 s, v3 a, v3 n, v3 pos) { return (m4){{{s.x, s.y, s.z, 0.0f}, {a.x, a.y, a.z, 0.0f}, {n.x, n.y, n.z, 0.0f}, {pos.x, pos.y, pos.z, 1.0f}}}; }

// gloved hand curled around a grip; `wrist` is the hand bone, `grip_axis` runs along the grip, `palm_normal` out of the palm
hz_internal void cab_glove(const CabNoise *nz, PgMesh *out, v3 wrist, v3 grip_axis, v3 palm_normal, f32 side) {
  v3 A = v3_normalize(grip_axis), N = v3_normalize(palm_normal);
  v3 S = v3_normalize(v3_cross(A, N));
  PgMesh *palm = cab_mesh(out->arena);
  cab_box_round(palm, 0.030f, 0.048f, 0.022f, 3.2f, 16, 7, 0.4f);
  v3 pos = v3_add(v3_add(wrist, v3_scale(A, 0.030f)), v3_scale(N, -0.006f));
  cab_compute_normals(palm);
  cab_transform(palm, cab_basis(S, A, N, pos));
  pg_mesh_append(out, palm);

  TempAllocator ta = tctx_temp_allocator_begin(out->arena);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, 5 * 10);
  for (u32 f = 0; f < 4; f++) {
    f32 t = (f32)f / 3.0f;
    f32 start_y = 0.052f - t * 0.030f;
    v3 *pts = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, 5);
    for (u32 i = 0; i <= 4; i++) {
      f32 u = (f32)i / 4.0f;
      f32 ang = u * 2.2f;
      f32 r = 0.030f - u * 0.004f;
      v3 p = wrist;
      p = v3_add(p, v3_scale(A, start_y - 0.004f + m_sinf(ang) * r * 0.55f));
      p = v3_add(p, v3_scale(N, -0.020f - (1.0f - m_cosf(ang)) * r * 0.9f));
      p = v3_add(p, v3_scale(S, side * (0.020f - t * 0.019f)));
      pts[i] = p;
      cab_ellipse_profile(prof + i * 10, 0.0115f - u * 0.002f, 0.0105f - u * 0.002f, 10);
    }
    PgMesh *fin = cab_mesh(out->arena);
    cab_tube(fin, pts, 5, prof, 10, CAB_UP_Z, true, true);
    cab_compute_normals(fin);
    pg_mesh_append(out, fin);
  }
  v3 *tp = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, 5);
  for (u32 i = 0; i <= 4; i++) {
    f32 u = (f32)i / 4.0f;
    v3 p = wrist;
    p = v3_add(p, v3_scale(A, 0.030f + u * 0.036f));
    p = v3_add(p, v3_scale(N, 0.006f - u * 0.026f));
    p = v3_add(p, v3_scale(S, side * (-0.026f - u * 0.004f)));
    tp[i] = p;
    cab_ellipse_profile(prof + i * 10, 0.014f - u * 0.003f, 0.013f - u * 0.003f, 10);
  }
  PgMesh *thumb = cab_mesh(out->arena);
  cab_tube(thumb, tp, 5, prof, 10, CAB_UP_Z, true, true);
  cab_compute_normals(thumb);
  pg_mesh_append(out, thumb);
  tctx_temp_allocator_end(ta);
  cab_compute_normals(out);
  CAB_DISPLACE(out, p, n, cab_fbm3(nz, p.x * 90.0f, p.y * 90.0f, p.z * 90.0f, 3) * 0.0012f);
}

// knuckle guard on the back of the glove
hz_internal void cab_knuckle(PgMesh *out, v3 wrist, v3 grip_axis, v3 palm_normal) {
  v3 A = v3_normalize(grip_axis), N = v3_normalize(palm_normal);
  v3 S = v3_normalize(v3_cross(A, N));
  cab_box_round(out, 0.026f, 0.024f, 0.007f, 3.4f, 14, 5, 0.5f);
  cab_compute_normals(out);
  cab_transform(out, cab_basis(S, A, N, v3_add(v3_add(wrist, v3_scale(A, 0.050f)), v3_scale(N, 0.020f))));
}

// --------------------------------------------------------------- weapon ---

#define CAB_BORE_Y 0.095f

// box in the weapon frame
hz_internal void cab_wbox(PgMesh *out, f32 hx, f32 hy, f32 hz, f32 x, f32 y, f32 z, f32 n, u32 rows, f32 round_y) {
  PgMesh *m = cab_mesh(out->arena);
  cab_box_round(m, hx, hy, hz, n, 16, rows, round_y);
  cab_compute_normals(m);
  pg_mesh_append(out, m);
  u32 first = out->pos.len - m->pos.len;
  pg_mesh_translate(out, (v3){x, y, z}, first);
}

// box with a custom segment count and a rotation
hz_internal void cab_wbox_ex(PgMesh *out, f32 hx, f32 hy, f32 hz, v3 pos, v3 rot, f32 n, u32 seg, u32 rows, f32 round_y) {
  PgMesh *m = cab_mesh(out->arena);
  cab_box_round(m, hx, hy, hz, n, seg, rows, round_y);
  cab_compute_normals(m);
  quat q = quat_mul(quat_mul(quat_axis_angle((v3){0.0f, 1.0f, 0.0f}, rot.y), quat_axis_angle((v3){1.0f, 0.0f, 0.0f}, rot.x)),
                    quat_axis_angle((v3){0.0f, 0.0f, 1.0f}, rot.z));
  cab_transform(m, m4_trs(pos, q, V3_ONE));
  pg_mesh_append(out, m);
}

// cylinder along +Z
hz_internal PgMesh *cab_cyl(ArenaAllocator *arena, f32 r0, f32 r1, f32 z0, f32 z1, f32 x, f32 y, u32 seg, b32 cap) {
  PgMesh *m = cab_mesh(arena);
  TempAllocator ta = tctx_temp_allocator_begin(arena);
  v3 *pts = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, 5);
  v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, 5 * seg);
  for (u32 i = 0; i < 5; i++) {
    f32 t = (f32)i / 4.0f;
    pts[i] = (v3){x, y, z0 + (z1 - z0) * (f32)i / 4.0f};
    f32 r = r0 + (r1 - r0) * t;
    cab_ellipse_profile(prof + i * seg, r, r, seg);
  }
  cab_tube(m, pts, 5, prof, seg, CAB_UP_Y, cap, cap);
  tctx_temp_allocator_end(ta);
  cab_compute_normals(m);
  return m;
}

hz_internal void cab_wcyl(PgMesh *out, f32 r0, f32 r1, f32 z0, f32 z1, f32 x, f32 y, u32 seg, b32 cap) {
  pg_mesh_append(out, cab_cyl(out->arena, r0, r1, z0, z1, x, y, seg, cap));
}

// the weapon frame to bind space: +Z down the bore, the firing wrist at the origin, a few degrees of cant
hz_internal m4 cab_weapon_matrix(f32 cant) {
  v3 bore = v3_normalize((v3){0.115f, -0.10f, 1.0f});
  v3 z = bore;
  v3 x = v3_normalize(v3_cross((v3){0.0f, 1.0f, 0.0f}, z));
  v3 y = v3_normalize(v3_cross(z, x));
  quat qc = quat_axis_angle(z, cant);
  x = quat_rotate(qc, x);
  y = quat_rotate(qc, y);
  return cab_basis(x, y, z, cod_rig_bind[COD_BONE_HAND_R]);
}

// the anchors the animator and the sling need, from the cant alone
void cab_weapon_anchors(CabWeapon *w, b32 long_weapon, f32 cant) {
  m4 m = cab_weapon_matrix(cant);
  f32 barrel_end = long_weapon ? 0.50f : 0.435f;
  w->muzzle = m4_mulv3(m, (v3){0.0f, CAB_BORE_Y, barrel_end + 0.012f}, 1.0f);
  w->bore_origin = m4_mulv3(m, (v3){0.0f, CAB_BORE_Y, 0.0f}, 1.0f);
  w->ejection = m4_mulv3(m, (v3){-0.024f, CAB_BORE_Y + 0.012f, 0.012f}, 1.0f);
  w->stock_top = m4_mulv3(m, (v3){0.0f, CAB_BORE_Y, -0.10f}, 1.0f);
  w->foregrip = m4_mulv3(m, (v3){0.0f, CAB_BORE_Y - 0.028f, long_weapon ? 0.22f : 0.205f}, 1.0f);
  w->mag_bottom = m4_mulv3(m, (v3){0.0f, CAB_BORE_Y - 0.25f, 0.03f}, 1.0f);
}

// the enemy rifle, modelled in its own frame and baked into the character mesh rigidly bound to the firing hand
void cab_weapon_build(const CabNoise *nz, CabVariant *v, ArenaAllocator *arena) {
  TempAllocator cab_tmp = tctx_temp_allocator_begin(NULL);
  CabWeapon *w = &v->weapon;
  PgMesh *steel = &w->steel, *poly = &w->polymer, *rubber = &w->rubber, *glass = &w->glass;
  pg_mesh_begin(steel, arena, 1024, 3072);
  pg_mesh_begin(poly, arena, 1024, 3072);
  pg_mesh_begin(rubber, arena, 128, 384);
  pg_mesh_begin(glass, arena, 128, 384);
  const b32 lg = v->def->long_weapon;
  const f32 barrel_end = lg ? 0.50f : 0.435f;
  const f32 B = CAB_BORE_Y;

  // lower receiver, magwell, trigger guard, pistol grip
  cab_wbox(poly, 0.019f, 0.031f, 0.055f, 0.0f, B - 0.036f, -0.012f, 4.2f, 7, 0.22f);
  cab_wbox(poly, 0.0165f, 0.030f, 0.028f, 0.0f, B - 0.052f, 0.004f, 4.5f, 7, 0.18f);
  {
    v3 *pts = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 9);
    for (u32 i = 0; i <= 8; i++) {
      f32 a = (f32)M_PI * ((f32)i / 8.0f);
      pts[i] = (v3){0.0f, B - 0.068f - m_sinf(a) * 0.020f, -0.028f + m_cosf(a) * -0.024f + 0.024f};
    }
    cab_add_ribbon(poly, pts, 9, 0.014f, 0.006f, 5, CAB_UP_X, false);
  }
  {
    const f32 rake = -0.38f;
    v3 *pts = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 6);
    v2 *prof = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v2, 6 * 14);
    for (u32 i = 0; i < 6; i++) {
      f32 t = (f32)i / 5.0f;
      pts[i] = (v3){0.0f, B - 0.052f - t * 0.105f, -0.028f - m_sinf(rake) * -t * 0.105f * 0.55f - t * 0.030f};
      cab_super_ellipse(prof + i * 14, 0.0165f - t * 0.002f, 0.020f - t * 0.004f, 3.4f, 14);
    }
    PgMesh *g = cab_mesh(arena);
    cab_tube(g, pts, 6, prof, 14, CAB_UP_Z, true, true);
    cab_compute_normals(g);
    CAB_DISPLACE(g, p, n, m_sinf((p.y - B) * 150.0f) * 0.0012f + cab_fbm3(nz, p.x * 90.0f, p.y * 90.0f, p.z * 90.0f, 2) * 0.0012f);
    pg_mesh_append(poly, g);
  }

  // magazine
  {
    const u32 rows = 9;
    CabRing *rings = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, CabRing, 9);
    v2 *prof = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v2, 9 * 14);
    for (u32 i = 0; i < rows; i++) {
      f32 t = (f32)i / (f32)(rows - 1);
      f32 y = B - 0.070f - t * (lg ? 0.20f : 0.175f);
      f32 z = 0.004f + t * t * (lg ? 0.062f : 0.030f);
      cab_super_ellipse(prof + i * 14, 0.0135f, 0.0225f - t * 0.001f, 4.4f, 14);
      rings[i] = (CabRing){.pts = prof + i * 14, .o = {0.0f, y, z}, .q = quat_axis_angle((v3){1.0f, 0.0f, 0.0f}, t * (lg ? 0.5f : 0.28f)), .s = {1.0f, 1.0f}};
    }
    PgMesh *mag = cab_mesh(arena);
    cab_loft(mag, rings, rows, 14, true, true);
    cab_compute_normals(mag);
    for (u32 _i = 0; _i < mag->pos.len; _i++) {
      v3 p = mag->pos.items[_i];
      v3 n = mag->nrm.items[_i];
      f32 rib = m_sinf((p.y - B) * 210.0f) * 0.5f + 0.5f;
      cab_push(mag, _i, m_fabsf(n.x) > 0.6f ? rib * 0.0012f : cab_fbm3(nz, p.x * 80.0f, p.y * 80.0f, p.z * 80.0f, 2) * 0.0008f);
    }
    pg_mesh_append(poly, mag);
  }

  // upper receiver, top rail with slots, ejection port cover, forward assist, charging handle
  cab_wbox(steel, 0.0175f, 0.0225f, 0.062f, 0.0f, B + 0.014f, 0.005f, 4.4f, 7, 0.22f);
  {
    f32 rail_z0 = -0.055f, rail_z1 = lg ? 0.06f : 0.145f;
    cab_wbox(steel, 0.0115f, 0.005f, (rail_z1 - rail_z0) * 0.5f, 0.0f, B + 0.040f, (rail_z0 + rail_z1) * 0.5f, 6.0f, 7, 0.1f);
    u32 n = (u32)m_floorf((rail_z1 - rail_z0) / 0.0102f);
    for (u32 i = 0; i < n; i++) {
      f32 z = rail_z0 + 0.004f + (f32)i * 0.0102f;
      cab_wbox(steel, 0.0125f, 0.0022f, 0.0026f, 0.0f, B + 0.0435f, z, 8.0f, 4, 0.1f);
    }
  }
  cab_wbox(steel, 0.0035f, 0.011f, 0.026f, -0.018f, B + 0.012f, 0.012f, 5.0f, 7, 0.2f);
  cab_wcyl(steel, 0.007f, 0.007f, -0.042f, -0.026f, -0.014f, B + 0.028f, 10, true);
  if (!lg) {
    cab_wbox(steel, 0.026f, 0.005f, 0.010f, 0.0f, B + 0.036f, -0.070f, 5.0f, 7, 0.3f);
    cab_wbox(steel, 0.010f, 0.010f, 0.004f, -0.026f, B + 0.036f, -0.070f, 5.0f, 7, 0.3f);
  } else {
    cab_wbox(steel, 0.010f, 0.008f, 0.020f, -0.024f, B + 0.026f, -0.020f, 5.0f, 7, 0.3f);
  }
  // barrel, gas block, muzzle device with port slots
  cab_wcyl(steel, 0.0105f, 0.0098f, 0.055f, barrel_end - 0.045f, 0.0f, B, 12, false);
  cab_wbox(steel, 0.0115f, 0.014f, 0.014f, 0.0f, B + 0.008f, lg ? 0.33f : 0.29f, 5.0f, 7, 0.25f);
  {
    PgMesh *mz = cab_cyl(arena, 0.0145f, 0.0135f, barrel_end - 0.045f, barrel_end, 0.0f, B, 14, true);
    for (u32 _i = 0; _i < mz->pos.len; _i++) {
      v3 p = mz->pos.items[_i];
      f32 a = m_atan2f(p.x, p.y - B);
      cab_push(mz, _i, (m_fabsf(m_sinf(a * 3.0f)) > 0.92f && p.z > barrel_end - 0.035f) ? -0.0035f : 0.0f);
    }
    pg_mesh_append(steel, mz);
  }

  // handguard
  {
    f32 z0 = 0.055f, z1 = lg ? 0.34f : 0.30f;
    const u32 rows = 7;
    CabRing *rings = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, CabRing, 7);
    v2 *prof = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v2, 7 * 18);
    quat qx = quat_axis_angle((v3){1.0f, 0.0f, 0.0f}, (f32)M_PI / 2.0f);
    for (u32 i = 0; i < rows; i++) {
      f32 t = (f32)i / (f32)(rows - 1);
      f32 r = lg ? 0.0255f - t * 0.002f : 0.0245f - t * 0.0025f;
      cab_super_ellipse(prof + i * 18, r, r * (lg ? 1.02f : 0.98f), lg ? 3.0f : 4.0f, 18);
      rings[i] = (CabRing){.pts = prof + i * 18, .o = {0.0f, B - 0.002f, z0 + (z1 - z0) * t}, .q = qx, .s = {1.0f, 1.0f}};
    }
    PgMesh *hg = cab_mesh(arena);
    cab_loft(hg, rings, rows, 18, false, true);
    cab_compute_normals(hg);
    for (u32 _i = 0; _i < hg->pos.len; _i++) {
      v3 p = hg->pos.items[_i];
      v3 n = hg->nrm.items[_i];
      f32 d;
      if (lg) {
      f32 rib = m_sinf(p.z * 260.0f) * 0.5f + 0.5f;
      d = rib * 0.0016f + cab_fbm3(nz, p.x * 70.0f, p.y * 70.0f, p.z * 70.0f, 2) * 0.001f;
      } else {
      b32 side = m_fabsf(n.x) > 0.55f;
      b32 down = n.y < -0.55f;
      f32 zm = p.z * 1000.0f;
      f32 slot = (zm - 42.0f * m_floorf(zm / 42.0f)) < 22.0f && (side || down) ? -0.0035f : 0.0f;
      d = slot + cab_fbm3(nz, p.x * 70.0f, p.y * 70.0f, p.z * 70.0f, 2) * 0.0008f;
      }
      cab_push(hg, _i, d);
    }
    pg_mesh_append(lg ? poly : steel, hg);
  }

  // stock
  if (lg) {
    for (i32 s = -1; s <= 1; s += 2) {
      f32 sf = (f32)s;
      v3 *pts = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, v3, 3);
      pts[0] = (v3){sf * 0.012f, B - 0.004f, -0.055f};
      pts[1] = (v3){sf * 0.016f, B - 0.016f, -0.135f};
      pts[2] = (v3){sf * 0.018f, B - 0.022f, -0.235f};
      cab_add_ribbon(steel, pts, 3, 0.010f, 0.008f, 6, CAB_UP_Y, false);
    }
    cab_wbox(rubber, 0.020f, 0.036f, 0.008f, 0.0f, B - 0.026f, -0.245f, 4.0f, 7, 0.4f);
    cab_wbox(poly, 0.020f, 0.026f, 0.030f, 0.0f, B - 0.018f, -0.085f, 4.0f, 7, 0.3f);
  } else {
    cab_wcyl(steel, 0.0155f, 0.0155f, -0.075f, -0.225f, 0.0f, B + 0.002f, 12, false);
    PgMesh *body = cab_mesh(arena);
    cab_box_round(body, 0.0215f, 0.030f, 0.058f, 3.8f, 16, 7, 0.28f);
    cab_compute_normals(body);
    pg_mesh_translate(body, (v3){0.0f, B - 0.004f, -0.175f}, 0);
    for (u32 i = 0; i < body->pos.len; i++)
      if (body->pos.items[i].y > B + 0.008f)
        body->pos.items[i].y -= (body->pos.items[i].z + 0.175f) * -0.12f;
    cab_compute_normals(body);
    CAB_DISPLACE(body, p, n, cab_fbm3(nz, p.x * 80.0f, p.y * 80.0f, p.z * 80.0f, 2) * 0.0012f);
    pg_mesh_append(poly, body);
    cab_wbox(poly, 0.0075f, 0.026f, 0.048f, 0.0f, B - 0.034f, -0.165f, 4.0f, 7, 0.3f);
    cab_wbox(rubber, 0.0215f, 0.033f, 0.007f, 0.0f, B - 0.004f, -0.230f, 4.0f, 7, 0.35f);
  }

  // sights and optic
  if (lg) {
    cab_wbox(steel, 0.010f, 0.010f, 0.006f, 0.0f, B + 0.026f, -0.040f, 5.0f, 7, 0.3f);
    cab_wbox(steel, 0.008f, 0.016f, 0.005f, 0.0f, B + 0.030f, 0.33f, 5.0f, 7, 0.3f);
  } else {
    cab_wbox(steel, 0.016f, 0.016f, 0.028f, 0.0f, B + 0.056f, 0.010f, 4.4f, 7, 0.25f);
    const f32 tz0 = -0.018f, tz1 = 0.058f;
    cab_wcyl(steel, 0.0205f, 0.0205f, tz0, tz1, 0.0f, B + 0.085f, 16, false);
    cab_wcyl(steel, 0.0225f, 0.0225f, tz0 - 0.006f, tz0 + 0.004f, 0.0f, B + 0.085f, 16, true);
    cab_wcyl(steel, 0.0225f, 0.0225f, tz1 - 0.004f, tz1 + 0.006f, 0.0f, B + 0.085f, 16, true);
    cab_wcyl(glass, 0.0185f, 0.0185f, tz0 - 0.0015f, tz0 + 0.0015f, 0.0f, B + 0.085f, 16, true);
    cab_wcyl(glass, 0.0185f, 0.0185f, tz1 - 0.0015f, tz1 + 0.0015f, 0.0f, B + 0.085f, 16, true);
    cab_wcyl(steel, 0.009f, 0.009f, 0.012f, 0.030f, -0.020f, B + 0.085f, 10, true);
  }
  // sling loops
  cab_wbox(steel, 0.005f, 0.010f, 0.006f, -0.016f, B - 0.020f, lg ? -0.06f : -0.072f, 5.0f, 7, 0.3f);
  cab_wbox(steel, 0.005f, 0.008f, 0.006f, -0.020f, B - 0.012f, lg ? 0.30f : 0.26f, 5.0f, 7, 0.3f);

  // bake into the actor's bind space
  m4 m = cab_weapon_matrix(v->cant);
  PgMesh **all = ALLOC_ARRAY_NO_ZERO(&cab_tmp.allocator, PgMesh *, 4);
  all[0] = steel;
  all[1] = poly;
  all[2] = rubber;
  all[3] = glass;
  for (u32 i = 0; i < 4; i++) {
    cab_compute_normals(all[i]);
    cab_transform(all[i], m);
    cab_compute_normals(all[i]);
    cab_weld_normals(all[i]);
  }
  cab_weapon_anchors(w, lg, v->cant);
  tctx_temp_allocator_end(cab_tmp);
}
