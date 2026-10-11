// Claude of Duty weapons: gloved hands, sleeved arms, two-bone solve and the fingertip contact fit.

typedef FixedArray(v2, 16) CwhProf;

// the finger specs: base x, segment lengths, joint radii
typedef struct CwhFingerSpec {
  f32 x;
  f32 len[3];
  f32 r[4];
} CwhFingerSpec;

hz_internal const CwhFingerSpec cwh_finger_spec[4] = {
    {0.0298f, {0.045f, 0.028f, 0.022f}, {0.0102f, 0.0096f, 0.0086f, 0.0062f}},
    {0.0102f, {0.049f, 0.031f, 0.023f}, {0.0104f, 0.0098f, 0.0088f, 0.0064f}},
    {-0.0104f, {0.046f, 0.029f, 0.022f}, {0.01f, 0.0094f, 0.0084f, 0.006f}},
    {-0.0298f, {0.038f, 0.024f, 0.02f}, {0.0092f, 0.0086f, 0.0078f, 0.0056f}},
};

#define CWH_THUMB_L0 0.05f
#define CWH_THUMB_L1 0.032f
#define CWH_THUMB_R0 0.0115f
#define CWH_THUMB_R1 0.0102f
#define CWH_THUMB_R2 0.0078f
#define CWH_L_UPPER 0.33f
#define CWH_L_FORE 0.3f

// the hand poses in CwhPoseId order: grip, wrap, clamp, cup, open, pinch
hz_internal const CwhPose cwh_poses[6] = {
    {{{0.55f, 0.72f, 0.34f}, {1.15f, 1.2f, 0.62f}, {1.2f, 1.25f, 0.65f}, {1.22f, 1.28f, 0.66f}}, {0.5f, 0.34f}, {0.15f, -1.02f, -0.62f}},
    {{{1.18f, 1.05f, 0.45f}, {1.26f, 1.12f, 0.5f}, {1.3f, 1.16f, 0.55f}, {1.34f, 1.2f, 0.6f}}, {0.42f, 0.3f}, {0.1f, -1.15f, -0.35f}},
    {{{0.612f, 1.059f, 0.797f}, {0.731f, 1.286f, 0.863f}, {0.73f, 1.268f, 0.808f}, {0.601f, 1.105f, 0.684f}}, {0.3f, 0.24f}, {0.04f, 0.76f, -0.05f}},
    {{{1.05f, 0.95f, 0.4f}, {1.12f, 1.0f, 0.44f}, {1.16f, 1.04f, 0.48f}, {1.2f, 1.08f, 0.52f}}, {0.28f, 0.2f}, {0.0f, -1.25f, -0.2f}},
    {{{0.35f, 0.28f, 0.14f}, {0.32f, 0.26f, 0.12f}, {0.34f, 0.28f, 0.14f}, {0.4f, 0.32f, 0.16f}}, {0.12f, 0.1f}, {0.1f, -0.8f, -0.35f}},
    {{{0.95f, 0.85f, 0.55f}, {1.0f, 0.9f, 0.6f}, {0.7f, 0.6f, 0.35f}, {0.6f, 0.5f, 0.3f}}, {0.62f, 0.55f}, {0.25f, -0.75f, -0.7f}},
};

// ------------------------------------------------------------- geometry kit ---

hz_internal void cwh_pt(CwhProf *p, f32 axial, f32 radius) { p->items[p->len++] = (v2){m_maxf(1e-5f, radius), axial}; }

hz_internal void cwh_rot_x(PgMesh *m, u32 from, f32 a) { pg_mesh_transform(m, m4_rotate(a, (v3){1.0f, 0.0f, 0.0f}), from); }
hz_internal void cwh_rot_y(PgMesh *m, u32 from, f32 a) { pg_mesh_transform(m, m4_rotate(a, (v3){0.0f, 1.0f, 0.0f}), from); }
hz_internal void cwh_rot_z(PgMesh *m, u32 from, f32 a) { pg_mesh_transform(m, m4_rotate(a, (v3){0.0f, 0.0f, 1.0f}), from); }
hz_internal void cwh_scale(PgMesh *m, u32 from, f32 x, f32 y, f32 z) { pg_mesh_transform(m, m4_scale((v3){x, y, z}), from); }

// a (axial, radius) profile revolved about +Z
hz_internal void cwh_lathe_z(PgMesh *m, const CwhProf *p, u32 seg) {
  u32 from = m->pos.len;
  pg_revolve(m, p->items, p->len, seg, 0.0f, 0.0f);
  cwh_rot_x(m, from, M_PIHALF);
}

// box / blob: a RoundedBox with the chamfer clamped to 49% of the shortest side, placed at t
hz_internal void cwh_box(PgMesh *m, f32 w, f32 h, f32 d, f32 chamfer, u32 seg, v3 t) {
  f32 r = m_minf(chamfer, m_minf(w, m_minf(h, d)) * 0.49f);
  u32 from = m->pos.len;
  pg_round_box(m, (v3){w, h, d}, seg, r);
  pg_mesh_translate(m, t, from);
}

// dome with cut 0.62 at 10 segments, facing +Y
hz_internal void cwh_dome(PgMesh *m, f32 radius, v3 t) {
  u32 from = m->pos.len;
  pg_sphere(m, &(PgSphereDesc){.radius = radius, .width_segments = 10, .height_segments = 5, .theta_length = M_PI * 0.62f});
  pg_mesh_translate(m, t, from);
}

// merges vertices equal within 1e-6 on position, normal and uv, compacting the arrays
hz_internal void cwh_weld(PgMesh *m) {
  u32 n = m->pos.len;
  if (n == 0)
    return;
  tctx_temp_allocator_scope(ta, m->arena) {
    Allocator *al = &ta.allocator;
    u32 tab_n = 64;
    while (tab_n < n * 2)
      tab_n <<= 1;
    u32 *table = ALLOC_ARRAY_NO_ZERO(al, u32, tab_n);
    for (u32 i = 0; i < tab_n; i++)
      table[i] = ~0u;
    i32 *keys = ALLOC_ARRAY_NO_ZERO(al, i32, n * 8);
    u32 *first = ALLOC_ARRAY_NO_ZERO(al, u32, n);
    u32 *remap = ALLOC_ARRAY_NO_ZERO(al, u32, n);
    u32 uniq = 0;
    for (u32 i = 0; i < n; i++) {
      v3 p = m->pos.items[i], q = m->nrm.items[i];
      v2 t = m->uv.items[i];
      // the candidate key is written into the next free slot and kept only if no earlier vertex matches
      i32 *k = keys + uniq * 8;
      k[0] = (i32)(p.x * 1e6f + 0.5f);
      k[1] = (i32)(p.y * 1e6f + 0.5f);
      k[2] = (i32)(p.z * 1e6f + 0.5f);
      k[3] = (i32)(q.x * 1e6f + 0.5f);
      k[4] = (i32)(q.y * 1e6f + 0.5f);
      k[5] = (i32)(q.z * 1e6f + 0.5f);
      k[6] = (i32)(t.x * 1e6f + 0.5f);
      k[7] = (i32)(t.y * 1e6f + 0.5f);
      u32 h = 2166136261u;
      for (u32 j = 0; j < 8; j++)
        h = (h ^ (u32)k[j]) * 16777619u;
      u32 s = h & (tab_n - 1);
      u32 found = ~0u;
      while (table[s] != ~0u) {
        const i32 *o = keys + table[s] * 8;
        if (o[0] == k[0] && o[1] == k[1] && o[2] == k[2] && o[3] == k[3] && o[4] == k[4] && o[5] == k[5] && o[6] == k[6] && o[7] == k[7]) {
          found = table[s];
          break;
        }
        s = (s + 1) & (tab_n - 1);
      }
      if (found == ~0u) {
        found = uniq++;
        first[found] = i;
        table[s] = found;
      }
      remap[i] = found;
    }
    // first[] ascends and first[u] >= u, so the in-place copy never reads a slot it already wrote
    for (u32 u = 0; u < uniq; u++) {
      u32 src = first[u];
      m->pos.items[u] = m->pos.items[src];
      m->nrm.items[u] = m->nrm.items[src];
      m->uv.items[u] = m->uv.items[src];
      m->mask.items[u] = m->mask.items[src];
    }
    m->pos.len = m->nrm.len = m->uv.len = m->mask.len = uniq;
    for (u32 i = 0; i < m->idx.len; i++)
      m->idx.items[i] = remap[m->idx.items[i]];
    m->charts.len = 0;
  }
}

// x -> -x with the winding flipped: the right hand's chirality baked into a mesh
hz_internal void cwh_mirror_x(PgMesh *m) {
  for (u32 i = 0; i < m->pos.len; i++) {
    m->pos.items[i].x = -m->pos.items[i].x;
    m->nrm.items[i].x = -m->nrm.items[i].x;
  }
  for (u32 t = 0; t + 2 < m->idx.len; t += 3) {
    u32 tmp = m->idx.items[t + 1];
    m->idx.items[t + 1] = m->idx.items[t + 2];
    m->idx.items[t + 2] = tmp;
  }
}

// ----------------------------------------------------------------- pieces ---

// one tapered finger segment with a joint crease, extending along -Z
hz_internal void cwh_segment(PgMesh *m, f32 len, f32 r0, f32 r1) {
  CwhProf p = {0};
  cwh_pt(&p, 0.0f, 0.0f);
  cwh_pt(&p, 0.0f, r0 * 0.86f);
  cwh_pt(&p, r0 * 0.5f, r0);
  cwh_pt(&p, len * 0.42f, r0 * 0.99f);
  cwh_pt(&p, len * 0.55f, r1 * 1.04f);
  cwh_pt(&p, len - r1 * 0.7f, r1);
  cwh_pt(&p, len - r1 * 0.2f, r1 * 0.8f);
  cwh_pt(&p, len, r1 * 0.35f);
  cwh_pt(&p, len, 0.0f);
  u32 from = m->pos.len;
  cwh_lathe_z(m, &p, 12);
  cwh_scale(m, from, 1.0f, 0.88f, 1.0f);
  cwh_rot_y(m, from, M_PI);
}

// the dorsal reinforcement
hz_internal void cwh_segment_pad(PgMesh *m, f32 len, f32 r) {
  cwh_box(m, r * 1.55f, r * 0.55f, len * 0.78f, r * 0.25f, 2, (v3){0.0f, r * 0.78f, -len * 0.46f});
}

// the sewn strip down one flank
hz_internal void cwh_segment_seam(PgMesh *m, f32 len, f32 r0, f32 r1, f32 sx) {
  cwh_box(m, 0.0015f, (r0 + r1) * 0.34f, len * 0.86f, 0.0003f, 1, (v3){sx * (r0 + r1) * 0.49f, r0 * 0.1f, -len * 0.47f});
}

// both flank seams of a segment as one geometry
hz_internal void cwh_segment_seams(PgMesh *m, f32 len, f32 r0, f32 r1) {
  cwh_segment_seam(m, len, r0, r1, 1.0f);
  cwh_segment_seam(m, len, r0, r1, -1.0f);
}

// palm, rear palm, thenar, heel and the four knuckle lumps
hz_internal void cwh_glove_shell(PgMesh *m, f32 sc) {
  f32 w = 0.088f * sc, h = 0.032f * sc, pl = 0.098f * sc;
  cwh_box(m, w, h, pl * 0.62f, 0.012f * sc, 3, (v3){0.0f, 0.0f, -pl * 0.66f});
  cwh_box(m, w * 0.83f, h * 0.96f, pl * 0.52f, 0.012f * sc, 3, (v3){0.0f, -h * 0.01f, -pl * 0.26f});
  cwh_box(m, w * 0.42f, h * 0.92f, pl * 0.6f, 0.014f * sc, 3, (v3){w * 0.3f, -h * 0.06f, -pl * 0.3f});
  cwh_box(m, w * 0.92f, h * 0.86f, 0.03f * sc, 0.012f * sc, 3, (v3){0.0f, -h * 0.04f, -0.012f * sc});
  for (u32 i = 0; i < 4; i++)
    cwh_dome(m, 0.0072f * sc, (v3){w * (0.34f - (f32)i * 0.225f), h * 0.42f, -pl * 0.94f});
}

// four knuckle caps, the metacarpal panel and the palm grip patch
hz_internal void cwh_glove_pads(PgMesh *m, f32 sc) {
  f32 w = 0.088f * sc, h = 0.032f * sc, pl = 0.098f * sc;
  for (u32 i = 0; i < 4; i++) {
    f32 x = w * (0.335f - (f32)i * 0.223f);
    f32 drop = m_fabsf((f32)i - 1.5f) > 1.0f ? h * 0.055f : 0.0f;
    cwh_box(m, w * 0.17f, h * 0.3f, pl * 0.3f, 0.005f * sc, 3, (v3){x, h * 0.46f - drop, -pl * 0.82f});
  }
  cwh_box(m, w * 0.44f, h * 0.17f, pl * 0.22f, 0.005f * sc, 3, (v3){0.0f, h * 0.44f, -pl * 0.4f});
  cwh_box(m, w * 0.82f, h * 0.18f, pl * 0.66f, 0.006f * sc, 3, (v3){0.0f, -h * 0.52f, -pl * 0.48f});
}

// the seams down both sides of the hand
hz_internal void cwh_glove_seams(PgMesh *m, f32 sc) {
  f32 w = 0.088f * sc, h = 0.032f * sc, pl = 0.098f * sc;
  cwh_box(m, 0.0016f * sc, h * 0.5f, pl * 0.8f, 0.0004f, 1, (v3){-w * 0.5f, 0.0f, -pl * 0.5f});
  cwh_box(m, 0.0016f * sc, h * 0.5f, pl * 0.8f, 0.0004f, 1, (v3){w * 0.5f, 0.0f, -pl * 0.5f});
}

// the wrist cuff (kind 0) and the strap (kind 1), an open lathe flattened to 0.82
hz_internal void cwh_glove_cuff(PgMesh *m, f32 sc, u32 kind) {
  f32 w = 0.088f * sc;
  CwhProf p = {0};
  if (kind == 0) {
    cwh_pt(&p, 0.0f, w * 0.44f);
    cwh_pt(&p, 0.004f * sc, w * 0.47f);
    cwh_pt(&p, 0.03f * sc, w * 0.46f);
    cwh_pt(&p, 0.034f * sc, w * 0.42f);
  } else {
    cwh_pt(&p, 0.0f, w * 0.47f);
    cwh_pt(&p, 0.0022f, w * 0.5f);
    cwh_pt(&p, 0.009f * sc, w * 0.5f);
    cwh_pt(&p, 0.0112f * sc, w * 0.47f);
  }
  u32 from = m->pos.len;
  cwh_lathe_z(m, &p, 16);
  cwh_scale(m, from, 1.0f, 0.82f, 1.0f);
}

// the grip patch on the fingertip segment
hz_internal void cwh_finger_tip(PgMesh *m, f32 len, f32 r) {
  cwh_box(m, r * 1.5f, r * 0.5f, len * 0.7f, r * 0.2f, 2, (v3){0.0f, -r * 0.72f, -len * 0.45f});
}

// the palmar grip patch and the dorsal nail plate of the distal segment
hz_internal void cwh_thumb_pad(PgMesh *m, f32 sc) {
  cwh_box(m, CWH_THUMB_R2 * 1.6f * sc, CWH_THUMB_R2 * 0.55f * sc, CWH_THUMB_L1 * 0.66f * sc, 0.0012f, 2,
          (v3){0.0f, -CWH_THUMB_R2 * 0.78f * sc, -CWH_THUMB_L1 * 0.45f * sc});
}

hz_internal void cwh_thumb_nail(PgMesh *m, f32 sc) {
  cwh_box(m, 0.011f * sc, 0.0035f * sc, 0.016f * sc, 0.0012f, 2, (v3){0.0f, CWH_THUMB_R2 * sc, -0.016f * sc});
}

// tapered sleeve with fold rings, wrinkle ridges, an optional elbow pad and rolled cuff, extending along -Z
hz_internal void cwh_sleeve(PgMesh *m, f32 len, f32 r0, f32 r1, u32 folds, b32 elbow_pad, b32 cuff) {
  const u32 seg = 32;
  CwhProf p = {0};
  cwh_pt(&p, 0.0f, 0.0f);
  cwh_pt(&p, 0.0f, r0 * 0.55f);
  cwh_pt(&p, -0.004f, r0 * 0.82f);
  cwh_pt(&p, -0.006f, r0 * 0.98f);
  cwh_pt(&p, 0.004f, r0);
  cwh_pt(&p, len * 0.16f, r0 * 1.03f);
  cwh_pt(&p, len * 0.34f, r0 * 0.9f);
  cwh_pt(&p, len * 0.52f, (r0 + r1) * 0.5f);
  cwh_pt(&p, len * 0.72f, r1 * 1.1f);
  cwh_pt(&p, len - 0.016f, r1 * 1.0f);
  cwh_pt(&p, len - 0.005f, r1 * 1.07f);
  cwh_pt(&p, len, r1 * 0.98f);
  cwh_pt(&p, len + 0.003f, r1 * 0.8f);
  cwh_pt(&p, len + 0.004f, 0.0f);
  cwh_lathe_z(m, &p, seg);

  // joint mass at the far end so the two bones read as one limb
  CwhProf j = {0};
  cwh_pt(&j, len - r1 * 1.1f, 0.0f);
  cwh_pt(&j, len - r1 * 0.9f, r1 * 0.75f);
  cwh_pt(&j, len - r1 * 0.2f, r1 * 1.04f);
  cwh_pt(&j, len + r1 * 0.5f, r1 * 0.9f);
  cwh_pt(&j, len + r1 * 0.8f, r1 * 0.4f);
  cwh_pt(&j, len + r1 * 0.85f, 0.0f);
  u32 from = m->pos.len;
  cwh_lathe_z(m, &j, 20);
  cwh_scale(m, from, 1.0f, 0.94f, 1.0f);

  // fold rings: ellipticity and a per-fold jitter so they do not read as a hose
  for (u32 i = 0; i < folds; i++) {
    f32 t = 0.14f + ((f32)i / m_maxf(1.0f, (f32)folds - 1.0f)) * 0.7f;
    f32 jit = m_sinf((f32)i * 2.399f + 0.7f) * 0.5f + m_sinf((f32)i * 5.13f) * 0.25f;
    f32 r = (r0 + (r1 - r0) * t) * (1.0f + jit * 0.06f);
    from = m->pos.len;
    pg_torus(m, &(PgTorusDesc){.radius = r * 0.985f, .tube = r * (0.085f + jit * 0.03f), .radial_segments = 6, .tubular_segments = 24});
    cwh_rot_x(m, from, M_PIHALF);
    cwh_rot_y(m, from, jit * 0.12f);
    cwh_scale(m, from, 1.0f, 0.93f, 1.0f);
    pg_mesh_translate(m, (v3){0.0f, 0.0f, len * t + jit * 0.004f}, from);
  }

  // longitudinal wrinkle ridges down both flanks
  for (u32 s = 0; s < 2; s++) {
    f32 sx = s == 0 ? -1.0f : 1.0f;
    CwhProf w = {0};
    cwh_pt(&w, len * 0.2f, 0.0f);
    cwh_pt(&w, len * 0.3f, r0 * 0.16f);
    cwh_pt(&w, len * 0.55f, r0 * 0.2f);
    cwh_pt(&w, len * 0.78f, r0 * 0.13f);
    cwh_pt(&w, len * 0.86f, 0.0f);
    from = m->pos.len;
    cwh_lathe_z(m, &w, 10);
    cwh_scale(m, from, 1.0f, 0.5f, 1.0f);
    cwh_rot_z(m, from, sx * 0.4f);
    pg_mesh_translate(m, (v3){sx * (r0 + r1) * 0.46f, -(r0 + r1) * 0.1f, 0.0f}, from);
  }
  if (elbow_pad)
    cwh_box(m, r0 * 1.5f, r0 * 0.6f, len * 0.3f, r0 * 0.3f, 3, (v3){0.0f, r0 * 0.75f, len * 0.12f});
  if (cuff) {
    CwhProf c = {0};
    cwh_pt(&c, len - 0.032f, r1 * 1.02f);
    cwh_pt(&c, len - 0.029f, r1 * 1.17f);
    cwh_pt(&c, len - 0.019f, r1 * 1.16f);
    cwh_pt(&c, len - 0.016f, r1 * 1.08f);
    cwh_pt(&c, len - 0.012f, r1 * 1.08f);
    cwh_pt(&c, len - 0.009f, r1 * 1.18f);
    cwh_pt(&c, len - 0.003f, r1 * 1.17f);
    cwh_pt(&c, len, r1 * 1.02f);
    cwh_lathe_z(m, &c, seg);
  }
  cwh_rot_y(m, 0, M_PI); // extend along -Z, like the bones
}

// ------------------------------------------------------------------ masks ---

typedef struct CwhMaskProf {
  v3 amp;
  v3 exp;
} CwhMaskProf;

// mask profiles per surface class: cloth, sleeve, pad, seam
hz_internal CwhMaskProf cwh_mask_prof(u32 mat) {
  switch (mat) {
  case CWP_MAT_SLEEVE: return (CwhMaskProf){{0.62f, 1.0f, 0.95f}, {1.5f, 1.0f, 1.0f}};
  case CWP_MAT_GLOVE_PAD: return (CwhMaskProf){{0.85f, 0.95f, 1.0f}, {2.2f, 1.4f, 1.2f}};
  case CWP_MAT_GLOVE_SEAM: return (CwhMaskProf){{1.0f, 0.7f, 0.8f}, {2.6f, 1.6f, 1.2f}};
  default: return (CwhMaskProf){{0.5f, 1.0f, 0.9f}, {1.6f, 1.15f, 1.1f}};
  }
}

// amp * v^exp on wear, grime and ao
hz_internal void cwh_shape_masks(PgMesh *m, CwhMaskProf p) {
  for (u32 i = 0; i < m->col.len; i++) {
    v4 c = m->col.items[i];
    c.x = c.x <= 0.0f ? 0.0f : p.amp.x * m_powf(m_minf(c.x, 1.0f), p.exp.x);
    c.y = c.y <= 0.0f ? 0.0f : p.amp.y * m_powf(m_minf(c.y, 1.0f), p.exp.y);
    c.z = c.z <= 0.0f ? 0.0f : p.amp.z * m_powf(m_minf(c.z, 1.0f), p.exp.z);
    m->col.items[i] = c;
  }
}

// ------------------------------------------------------------------ build ---

typedef struct CwhBuild {
  CwpArm *a;
  ArenaAllocator *arena;
  CodRng rng;
} CwhBuild;

hz_internal CwpArmMesh *cwh_dest(CwhBuild *b, u32 node, u32 mat) {
  CwpArm *a = b->a;
  for (u32 i = 0; i < a->mesh_count; i++)
    if (a->mesh[i].node == node && a->mesh[i].mat == mat)
      return &a->mesh[i];
  assert_msg(a->mesh_count < CWH_MAX_MESHES, "cwh: mesh capacity");
  CwpArmMesh *d = &a->mesh[a->mesh_count++];
  d->node = node;
  d->mat = mat;
  pg_mesh_begin(&d->mesh, b->arena, 4096, 16384);
  return d;
}

// finishes one hand geometry: weld, bake and shape its masks, offset it, mirror for the right hand, merge into (node, material)
hz_internal void cwh_emit(CwhBuild *b, PgMesh *s, u32 node, u32 mat, v3 shift) {
  cwh_weld(s);
  CodMaskOpts o = cod_mask_opts_default();
  o.edge_threshold = 0.09f;
  cod_bake_masks(s, &o, &b->rng);
  cwh_shape_masks(s, cwh_mask_prof(mat));
  pg_mesh_translate(s, shift, 0);
  if (b->a->side > 0 && node >= CWH_NODE_HAND)
    cwh_mirror_x(s);
  pg_mesh_append(&cwh_dest(b, node, mat)->mesh, s);
}

hz_internal void cwh_scratch_init(PgMesh *m, TempAllocator *ta) { pg_mesh_begin(m, ta->temp.arena, 16384, 65536); }

// a scratch mesh `s` on the lane's temp arena for the body, emitted into (node, mat) when the body ends
#define CWH_PIECE(b, s, node, mat, shift)                                                                   \
  for (TempAllocator _ta = tctx_temp_allocator_begin((b)->arena), *_once = &_ta; _once;                    \
       tctx_temp_allocator_end(_ta), _once = NULL)                                                          \
    for (PgMesh s = {0}; _once && (cwh_scratch_init(&s, &_ta), true); cwh_emit((b), &s, (node), (mat), (shift)), _once = NULL)

// every hand geometry in build order (the rng stream follows it)
hz_internal void cwh_build_pieces(CwhBuild *b) {
  CwpArm *a = b->a;
  f32 sc = a->scale;
  CWH_PIECE(b, s, CWH_NODE_UPPER, CWP_MAT_SLEEVE, V3_ZERO) cwh_sleeve(&s, a->l1, 0.044f * sc, 0.036f * sc, 5, true, false);
  CWH_PIECE(b, s, CWH_NODE_FORE, CWP_MAT_SLEEVE, V3_ZERO) cwh_sleeve(&s, a->l2, 0.034f * sc, 0.024f * sc, 7, false, true);
  CWH_PIECE(b, s, CWH_NODE_HAND, CWP_MAT_GLOVE, V3_ZERO) cwh_glove_shell(&s, sc);
  CWH_PIECE(b, s, CWH_NODE_HAND, CWP_MAT_GLOVE_PAD, V3_ZERO) cwh_glove_pads(&s, sc);
  CWH_PIECE(b, s, CWH_NODE_HAND, CWP_MAT_GLOVE_PAD, V3_ZERO) cwh_glove_seams(&s, sc);
  CWH_PIECE(b, s, CWH_NODE_HAND, CWP_MAT_GLOVE, ((v3){0.0f, 0.0f, 0.004f * sc})) cwh_glove_cuff(&s, sc, 0);
  CWH_PIECE(b, s, CWH_NODE_HAND, CWP_MAT_GLOVE_PAD, ((v3){0.0f, 0.0f, 0.02f * sc})) cwh_glove_cuff(&s, sc, 1);
  for (u32 i = 0; i < 4; i++) {
    for (u32 j = 0; j < 3; j++) {
      u32 node = CWH_NODE_FINGER0 + 4 * i + 1 + j;
      f32 len = a->seg_l[i][j], r0 = a->seg_r[i][j], r1 = a->seg_r[i][j + 1];
      CWH_PIECE(b, s, node, CWP_MAT_GLOVE, V3_ZERO) cwh_segment(&s, len, r0, r1);
      if (j < 2) {
        CWH_PIECE(b, s, node, CWP_MAT_GLOVE_SEAM, V3_ZERO) cwh_segment_seams(&s, len, r0, r1);
        CWH_PIECE(b, s, node, CWP_MAT_GLOVE_PAD, V3_ZERO) cwh_segment_pad(&s, len, r0);
      } else {
        CWH_PIECE(b, s, node, CWP_MAT_GLOVE_PAD, V3_ZERO) cwh_finger_tip(&s, len, r0);
      }
    }
  }
  f32 l0 = CWH_THUMB_L0 * sc, l1 = CWH_THUMB_L1 * sc, r0 = CWH_THUMB_R0 * sc, r1 = CWH_THUMB_R1 * sc, r2 = CWH_THUMB_R2 * sc;
  CWH_PIECE(b, s, CWH_NODE_THUMB + 1, CWP_MAT_GLOVE, V3_ZERO) cwh_segment(&s, l0, r0, r1);
  CWH_PIECE(b, s, CWH_NODE_THUMB + 1, CWP_MAT_GLOVE_PAD, V3_ZERO) cwh_segment_pad(&s, l0, r0);
  CWH_PIECE(b, s, CWH_NODE_THUMB + 1, CWP_MAT_GLOVE_SEAM, V3_ZERO) cwh_segment_seams(&s, l0, r0, r1);
  CWH_PIECE(b, s, CWH_NODE_THUMB + 2, CWP_MAT_GLOVE, V3_ZERO) cwh_segment(&s, l1, r1, r2);
  CWH_PIECE(b, s, CWH_NODE_THUMB + 2, CWP_MAT_GLOVE_PAD, V3_ZERO) cwh_thumb_pad(&s, sc);
  CWH_PIECE(b, s, CWH_NODE_THUMB + 2, CWP_MAT_GLOVE_PAD, V3_ZERO) cwh_thumb_nail(&s, sc);
}

// ---------------------------------------------------------------- the rig ---

// writes the actual node TRS from the unmirrored one: below the hand the chirality mirror conjugates it
hz_internal void cwh_sync(CwpArm *a, u32 i) {
  v3 p = a->rpos[i];
  quat q = a->rrot[i];
  if (a->side > 0 && i >= CWH_NODE_FINGER0) {
    p.x = -p.x;
    q.y = -q.y;
    q.z = -q.z;
  }
  a->node[i].pos = p;
  a->node[i].rot = q;
}

hz_internal void cwh_set_node(CwpArm *a, u32 i, i32 parent, v3 pos, quat rot) {
  a->node[i].parent = parent;
  a->node[i].scale = (v3){1.0f, 1.0f, 1.0f};
  a->rpos[i] = pos;
  a->rrot[i] = rot;
  cwh_sync(a, i);
}

hz_internal quat cwh_rot_x_quat(f32 angle) { return quat_axis_angle((v3){1.0f, 0.0f, 0.0f}, angle); }

// writes every finger and thumb joint node from the joint state
hz_internal void cwh_sync_joints(CwpArm *a) {
  for (u32 i = 0; i < 4; i++)
    for (u32 j = 0; j < 3; j++) {
      u32 n = CWH_NODE_FINGER0 + 4 * i + 1 + j;
      a->rrot[n] = cwh_rot_x_quat(a->fj[i][j]);
      cwh_sync(a, n);
    }
  a->rrot[CWH_NODE_THUMB] = quat_from_euler(a->tbase);
  cwh_sync(a, CWH_NODE_THUMB);
  for (u32 k = 0; k < 2; k++) {
    a->rrot[CWH_NODE_THUMB + 1 + k] = cwh_rot_x_quat(a->tj[k]);
    cwh_sync(a, CWH_NODE_THUMB + 1 + k);
  }
}

hz_internal void cwh_init_rig(CwpArm *a) {
  f32 sc = a->scale;
  a->node_count = CWH_NODE_COUNT;
  quat id = quat_identity();
  cwh_set_node(a, CWH_NODE_ROOT, -1, V3_ZERO, id);
  cwh_set_node(a, CWH_NODE_UPPER, CWH_NODE_ROOT, V3_ZERO, id);
  cwh_set_node(a, CWH_NODE_FORE, CWH_NODE_ROOT, V3_ZERO, id);
  cwh_set_node(a, CWH_NODE_HAND, CWH_NODE_ROOT, V3_ZERO, id);
  for (u32 i = 0; i < 4; i++) {
    const CwhFingerSpec *sp = &cwh_finger_spec[i];
    for (u32 k = 0; k < 4; k++)
      a->seg_r[i][k] = sp->r[k] * sc;
    for (u32 k = 0; k < 3; k++)
      a->seg_l[i][k] = sp->len[k] * sc;
    u32 root = CWH_NODE_FINGER0 + 4 * i;
    // the MCP row sits on the palmar half of the hand and the fingers fan out slightly
    cwh_set_node(a, root, CWH_NODE_HAND, (v3){sp->x * sc, -0.006f * sc, -0.096f * sc}, quat_axis_angle((v3){0.0f, 1.0f, 0.0f}, -sp->x * 2.2f));
    cwh_set_node(a, root + 1, (i32)root, V3_ZERO, id);
    cwh_set_node(a, root + 2, (i32)root + 1, (v3){0.0f, 0.0f, -a->seg_l[i][0]}, id);
    cwh_set_node(a, root + 3, (i32)root + 2, (v3){0.0f, 0.0f, -a->seg_l[i][1]}, id);
  }
  cwh_set_node(a, CWH_NODE_THUMB, CWH_NODE_HAND, (v3){0.037f * sc, -0.009f * sc, -0.04f * sc}, id);
  cwh_set_node(a, CWH_NODE_THUMB + 1, CWH_NODE_THUMB, V3_ZERO, id);
  cwh_set_node(a, CWH_NODE_THUMB + 2, CWH_NODE_THUMB + 1, (v3){0.0f, 0.0f, -CWH_THUMB_L0 * sc}, id);
}

void cwh_arm_build(CwpArm *a, i32 side, f32 scale, ArenaAllocator *arena) {
  mem_zero(a, sizeof(*a));
  a->side = side;
  a->scale = scale;
  a->l1 = CWH_L_UPPER * scale;
  a->l2 = CWH_L_FORE * scale;
  // elbows swing down and outboard in rig space, never in hand space
  a->pole = v3_normalize((v3){(f32)side * 0.46f, -0.86f, 0.22f});
  cwh_init_rig(a);
  CwhBuild b = {.a = a, .arena = arena, .rng = cod_rng_make(0xa2a00000u + (side > 0 ? 1u : 2u))};
  cwh_build_pieces(&b);
  cwh_arm_set_pose(a, CWH_POSE_WRAP);
}

u32 cwh_arm_fit_pose_slot(u32 weapon) { return CWH_POSE_FIT0 + (weapon < CWH_FIT_SLOTS ? weapon : CWH_FIT_SLOTS - 1); }

// the pose's finger and thumb curls: a fitted slot, a hand pose entry, or wrap
hz_internal CwhPose cwh_pose_def(const CwpArm *a, u32 pose) {
  if (pose >= CWH_POSE_FIT0 && pose < CWH_POSE_COUNT) {
    u32 slot = pose - CWH_POSE_FIT0;
    return a->fitted_valid[slot] ? a->fitted[slot] : cwh_poses[CWH_POSE_WRAP];
  }
  return cwh_poses[pose < 6 ? pose : CWH_POSE_WRAP];
}

void cwh_arm_set_pose(CwpArm *a, u32 pose) {
  CwhPose p = cwh_pose_def(a, pose);
  for (u32 i = 0; i < 4; i++)
    for (u32 j = 0; j < 3; j++)
      a->fj[i][j] = -p.fingers[i][j];
  a->tj[0] = -p.thumb[0];
  a->tj[1] = -p.thumb[1];
  a->tbase = p.thumb_base;
  a->pose = pose;
  cwh_sync_joints(a);
}

// the index finger rides the trigger: the grip pose's rest values plus the press
void cwh_arm_set_trigger(CwpArm *a, f32 t) {
  a->fj[0][0] = -(0.55f + t * 0.3f);
  a->fj[0][1] = -(0.72f + t * 0.42f);
  a->fj[0][2] = -(0.34f + t * 0.3f);
  for (u32 j = 0; j < 3; j++) {
    a->rrot[CWH_NODE_FINGER0 + 1 + j] = cwh_rot_x_quat(a->fj[0][j]);
    cwh_sync(a, CWH_NODE_FINGER0 + 1 + j);
  }
}

// hand +Z against the fingers, +Y toward the back of the hand
quat cwh_hand_basis(v3 finger, v3 back) {
  v3 z = v3_normalize(v3_negate(finger));
  v3 y = v3_sub(back, v3_scale(z, v3_dot(back, z)));
  if (v3_lengthsq(y) < 1e-8f)
    y = v3_sub((v3){0.0f, 1.0f, 0.0f}, v3_scale(z, z.y));
  y = v3_normalize(y);
  v3 x = v3_normalize(v3_cross(y, z));
  return quat_from_m3((m3){{{x.x, x.y, x.z}, {y.x, y.y, y.z}, {z.x, z.y, z.z}}});
}

// orients a bone whose geometry runs along local -Z so -Z points along dir, +Y rolled toward up
hz_internal quat cwh_aim_bone(v3 dir, v3 up) {
  v3 bz = v3_normalize(v3_negate(dir));
  v3 by = v3_sub(up, v3_scale(bz, v3_dot(up, bz)));
  if (v3_lengthsq(by) < 1e-9f) {
    by = v3_sub((v3){0.0f, 1.0f, 0.0f}, v3_scale(bz, bz.y));
    if (v3_lengthsq(by) < 1e-9f)
      by = v3_sub((v3){1.0f, 0.0f, 0.0f}, v3_scale(bz, bz.x));
  }
  by = v3_normalize(by);
  v3 bx = v3_normalize(v3_cross(by, bz));
  return quat_from_m3((m3){{{bx.x, bx.y, bx.z}, {by.x, by.y, by.z}, {bz.x, bz.y, bz.z}}});
}

// two-bone solve: the hand lands on the target, the elbow swings toward the pole
void cwh_arm_solve(CwpArm *a, v3 shoulder, v3 target_pos, quat target_quat) {
  a->rpos[CWH_NODE_HAND] = target_pos;
  a->rrot[CWH_NODE_HAND] = target_quat;
  cwh_sync(a, CWH_NODE_HAND);

  v3 t = v3_sub(target_pos, shoulder);
  f32 d = v3_length(t);
  f32 max_d = (a->l1 + a->l2) * 0.995f;
  f32 min_d = m_fabsf(a->l1 - a->l2) * 1.05f + 1e-4f;
  if (d > max_d) {
    t = v3_scale(t, max_d / d);
    d = max_d;
  } else if (d < min_d) {
    t = d < 1e-5f ? (v3){0.0f, 0.0f, -min_d} : v3_scale(t, min_d / d);
    d = min_d;
  }
  v3 dir = v3_scale(t, 1.0f / d);

  // the circle of possible elbow positions; pick the point toward the pole
  f32 aa = (a->l1 * a->l1 - a->l2 * a->l2 + d * d) / (2.0f * d);
  f32 h = m_sqrtf(m_maxf(0.0f, a->l1 * a->l1 - aa * aa));
  v3 perp = v3_sub(a->pole, v3_scale(dir, v3_dot(a->pole, dir)));
  if (v3_lengthsq(perp) < 1e-8f) {
    perp = (v3){(f32)a->side, -1.0f, 0.0f};
    perp = v3_sub(perp, v3_scale(dir, v3_dot(perp, dir)));
  }
  perp = v3_normalize(perp);
  v3 elbow = v3_add(v3_add(shoulder, v3_scale(dir, aa)), v3_scale(perp, h));

  a->rpos[CWH_NODE_UPPER] = shoulder;
  v3 hp = v3_sub(elbow, shoulder);
  if (v3_lengthsq(hp) > 1e-12f)
    a->rrot[CWH_NODE_UPPER] = cwh_aim_bone(hp, perp);
  cwh_sync(a, CWH_NODE_UPPER);

  a->rpos[CWH_NODE_FORE] = elbow;
  v3 up = quat_rotate(target_quat, (v3){0.0f, 1.0f, 0.0f});
  hp = v3_sub(target_pos, elbow);
  if (v3_lengthsq(hp) > 1e-12f)
    a->rrot[CWH_NODE_FORE] = cwh_aim_bone(hp, up);
  cwh_sync(a, CWH_NODE_FORE);
}

// ------------------------------------------------------------ contact fit ---

// a node's rig-space pose through the actual (mirror-conjugated) chain, composed from the node outward
void cwh_arm_node_world(const CwpArm *a, u32 node, v3 *pos, quat *rot) {
  v3 p = V3_ZERO;
  quat q = quat_identity();
  i32 n = (i32)node;
  while (n >= 0) {
    p = v3_add(a->node[n].pos, quat_rotate(a->node[n].rot, p));
    q = quat_mul(a->node[n].rot, q);
    n = a->node[n].parent;
  }
  *pos = p;
  *rot = q;
}

// a joint-local point in rig space
hz_internal v3 cwh_joint_point(const CwpArm *a, u32 node, v3 local) {
  if (a->side > 0)
    local.x = -local.x;
  v3 p = local;
  i32 n = (i32)node;
  while (n >= 0) {
    p = v3_add(a->node[n].pos, quat_rotate(a->node[n].rot, p));
    n = a->node[n].parent;
  }
  return p;
}

typedef struct CwhCyl {
  v3 ax0;
  v3 axis;
  f32 radius;
} CwhCyl;

hz_internal f32 cwh_gap(const CwpArm *a, const CwhCyl *c, u32 node, v3 local, v3 *out_point) {
  v3 p = cwh_joint_point(a, node, local);
  if (out_point)
    *out_point = p;
  v3 d = v3_sub(p, c->ax0);
  d = v3_sub(d, v3_scale(c->axis, v3_dot(d, c->axis)));
  return v3_length(d) - c->radius;
}

// scans one joint's flexion for the angle that puts `local` on the surface (a scan: the gap is not monotonic in curl)
hz_internal f32 cwh_fit_joint(CwpArm *a, const CwhCyl *c, u32 node, f32 *rotx, v3 local, f32 lo, f32 hi, f32 standoff, f32 clearance) {
  f32 best = *rotx;
  f32 best_cost = HZ_INF;
  for (u32 i = 0; i <= 48; i++) {
    f32 ang = lo + ((hi - lo) * (f32)i) / 48.0f;
    *rotx = ang;
    a->rrot[node] = cwh_rot_x_quat(ang);
    cwh_sync(a, node);
    f32 g = cwh_gap(a, c, node, local, NULL) - standoff;
    // on the surface, up to `clearance` proud, at most 1.5 mm buried
    f32 cost = m_fabsf(g - clearance * 0.5f) + (g < -0.0015f ? (-g - 0.0015f) * 8.0f : 0.0f);
    if (cost < best_cost) {
      best_cost = cost;
      best = ang;
    }
  }
  *rotx = best;
  a->rrot[node] = cwh_rot_x_quat(best);
  cwh_sync(a, node);
  return best;
}

u32 cwh_arm_fit_to_cylinder(CwpArm *a, v3 hand_pos, quat hand_quat, v3 axis_point, v3 axis_dir, f32 radius, f32 clearance,
                            u32 fitted_slot, v3 *contacts_out, u32 max_contacts) {
  assert_msg(fitted_slot >= CWH_POSE_FIT0 && fitted_slot < CWH_POSE_COUNT, "cwh: fit slot");
  CwhPose base = cwh_poses[CWH_POSE_CLAMP];
  a->rpos[CWH_NODE_HAND] = hand_pos;
  a->rrot[CWH_NODE_HAND] = hand_quat;
  cwh_sync(a, CWH_NODE_HAND);
  CwhCyl cyl = {.ax0 = axis_point, .axis = v3_normalize(axis_dir), .radius = radius};
  CwhPose out = base;
  u32 count = 0;

  // proximal first: each joint places the next one's origin a finger radius off the surface, the distal joint the contact patch
  for (u32 i = 0; i < 4; i++) {
    for (u32 j = 0; j < 3; j++)
      a->fj[i][j] = -base.fingers[i][j];
    cwh_sync_joints(a);
    for (u32 j = 0; j < 2; j++) {
      f32 ang = cwh_fit_joint(a, &cyl, CWH_NODE_FINGER0 + 4 * i + 1 + j, &a->fj[i][j], (v3){0.0f, 0.0f, -a->seg_l[i][j]}, -1.75f, -0.05f,
                              a->seg_r[i][j + 1] * 0.92f, clearance);
      out.fingers[i][j] = -ang;
    }
    v3 tip = {0.0f, -a->seg_r[i][3] * 1.05f, -a->seg_l[i][2] * 0.5f};
    f32 ang = cwh_fit_joint(a, &cyl, CWH_NODE_FINGER0 + 4 * i + 3, &a->fj[i][2], tip, -1.95f, -0.1f, 0.0f, clearance);
    out.fingers[i][2] = -ang;
    v3 p;
    cwh_gap(a, &cyl, CWH_NODE_FINGER0 + 4 * i + 3, tip, &p);
    if (count < max_contacts)
      contacts_out[count] = p;
    count++;
  }

  // thumb: over the top and down the far side; its base is searched in two axes before the hinges are fitted
  a->tbase = base.thumb_base;
  a->tj[0] = -base.thumb[0];
  a->tj[1] = -base.thumb[1];
  cwh_sync_joints(a);
  f32 tr = CWH_THUMB_R2 * a->scale, tlen = CWH_THUMB_L1 * a->scale;
  v3 t_local = {0.0f, -tr * 1.05f, -tlen * 0.55f};
  a->tj[0] = -0.55f;
  a->tj[1] = -0.45f;
  cwh_sync_joints(a);
  f32 y0 = base.thumb_base.y, z0 = base.thumb_base.z;
  f32 best_y = y0, best_z = z0, best_cost = HZ_INF;
  for (u32 i = 0; i <= 20; i++) {
    f32 yy = y0 - 1.3f + (2.6f * (f32)i) / 20.0f;
    for (u32 k = 0; k <= 14; k++) {
      f32 zz = z0 - 0.9f + (1.8f * (f32)k) / 14.0f;
      a->tbase.y = yy;
      a->tbase.z = zz;
      a->rrot[CWH_NODE_THUMB] = quat_from_euler(a->tbase);
      cwh_sync(a, CWH_NODE_THUMB);
      f32 g = cwh_gap(a, &cyl, CWH_NODE_THUMB + 2, t_local, NULL);
      f32 cost = m_fabsf(g - clearance) + (g < -0.002f ? (-g - 0.002f) * 10.0f : 0.0f) + (m_fabsf(yy - y0) + m_fabsf(zz - z0)) * 0.0009f;
      if (cost < best_cost) {
        best_cost = cost;
        best_y = yy;
        best_z = zz;
      }
    }
  }
  a->tbase.y = best_y;
  a->tbase.z = best_z;
  out.thumb_base = a->tbase;
  a->rrot[CWH_NODE_THUMB] = quat_from_euler(a->tbase);
  cwh_sync(a, CWH_NODE_THUMB);
  f32 a0 = cwh_fit_joint(a, &cyl, CWH_NODE_THUMB + 1, &a->tj[0], (v3){0.0f, 0.0f, -CWH_THUMB_L0 * a->scale}, -1.45f, -0.02f,
                         CWH_THUMB_R1 * a->scale, clearance);
  out.thumb[0] = -a0;
  f32 a1 = cwh_fit_joint(a, &cyl, CWH_NODE_THUMB + 2, &a->tj[1], t_local, -1.6f, -0.05f, 0.0f, clearance);
  out.thumb[1] = -a1;
  v3 tp;
  cwh_gap(a, &cyl, CWH_NODE_THUMB + 2, t_local, &tp);
  if (count < max_contacts)
    contacts_out[count] = tp;
  count++;

  u32 slot = fitted_slot - CWH_POSE_FIT0;
  a->fitted[slot] = out;
  a->fitted_valid[slot] = true;
  a->pose = fitted_slot;
  return count < max_contacts ? count : max_contacts;
}

// contact AO on the glove side: smootherstep falloff from each contact, raised into the AO channel only
void cwh_arm_bake_contact_ao(CwpArm *a, const v3 *contacts, u32 count, f32 radius, f32 peak) {
  if (count == 0)
    return;
  f32 r2 = radius * radius;
  for (u32 mi = 0; mi < a->mesh_count; mi++) {
    CwpArmMesh *am = &a->mesh[mi];
    if (am->node < CWH_NODE_HAND)
      continue;
    PgMesh *m = &am->mesh;
    pg_mesh_enable_streams(m, PG_STREAM_COLOR);
    v3 wp;
    quat wq;
    cwh_arm_node_world(a, am->node, &wp, &wq);
    for (u32 i = 0; i < m->pos.len; i++) {
      v3 p = v3_add(wp, quat_rotate(wq, m->pos.items[i]));
      f32 closest = HZ_INF;
      for (u32 c = 0; c < count; c++)
        closest = m_minf(closest, v3_distancesq(p, contacts[c]));
      if (closest > r2)
        continue;
      f32 t = 1.0f - m_sqrtf(closest) / radius;
      f32 s = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
      m->col.items[i].z = m_maxf(m->col.items[i].z, peak * s);
    }
  }
}

