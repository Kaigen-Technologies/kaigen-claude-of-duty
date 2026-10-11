// Curvature vertex masks: convex edges wear, concave creases collect grime and AO.

void cod_bake_masks(PgMesh *m, const CodMaskOpts *opts, CodRng *rng) {
  u32 count = m->pos.len;
  if (count == 0)
    return;
  pg_mesh_enable_streams(m, PG_STREAM_COLOR);
  CodMaskOpts o = opts ? *opts : cod_mask_opts_default();
  u32 tri_count = m->idx.len ? m->idx.len / 3 : count / 3;
  const u32 *idx = m->idx.len ? m->idx.items : NULL;

  tctx_temp_allocator_scope(ta, m->arena) {
    Allocator *al = &ta.allocator;
    u32 *cluster = ALLOC_ARRAY_NO_ZERO(al, u32, count);
    i32 *qx = ALLOC_ARRAY_NO_ZERO(al, i32, count);
    i32 *qy = ALLOC_ARRAY_NO_ZERO(al, i32, count);
    i32 *qz = ALLOC_ARRAY_NO_ZERO(al, i32, count);
    u32 tab_n = 64;
    while (tab_n < count * 2)
      tab_n <<= 1;
    u32 *table = ALLOC_ARRAY_NO_ZERO(al, u32, tab_n);
    for (u32 i = 0; i < tab_n; i++)
      table[i] = ~0u;
    // positions are clustered by their 1/8192 quantisation so hard-edged kit geometry sees across its seams
    u32 clusters = 0;
    for (u32 i = 0; i < count; i++) {
      v3 p = m->pos.items[i];
      i32 x = (i32)m_roundf(p.x * 8192.0f), y = (i32)m_roundf(p.y * 8192.0f), z = (i32)m_roundf(p.z * 8192.0f);
      u32 h = ((u32)x * 73856093u) ^ ((u32)y * 19349663u) ^ ((u32)z * 83492791u);
      u32 s = h & (tab_n - 1);
      u32 found = ~0u;
      while (table[s] != ~0u) {
        u32 c = table[s];
        if (qx[c] == x && qy[c] == y && qz[c] == z) {
          found = c;
          break;
        }
        s = (s + 1) & (tab_n - 1);
      }
      if (found == ~0u) {
        found = clusters++;
        qx[found] = x;
        qy[found] = y;
        qz[found] = z;
        table[s] = found;
      }
      cluster[i] = found;
    }

    v3 *sum_off = ALLOC_ARRAY(al, v3, clusters);
    v3 *cnx = ALLOC_ARRAY(al, v3, clusters);
    f32 *hits = ALLOC_ARRAY(al, f32, clusters);
    f32 *ncnt = ALLOC_ARRAY(al, f32, clusters);
    for (u32 t = 0; t < tri_count; t++) {
      u32 base = t * 3;
      for (u32 k = 0; k < 3; k++) {
        u32 i0 = idx ? idx[base + k] : base + k;
        u32 i1 = idx ? idx[base + (k + 1) % 3] : base + (k + 1) % 3;
        u32 i2 = idx ? idx[base + (k + 2) % 3] : base + (k + 2) % 3;
        v3 p0 = m->pos.items[i0];
        u32 c = cluster[i0];
        v3 u = v3_sub(m->pos.items[i1], p0);
        v3 v = v3_sub(m->pos.items[i2], p0);
        f32 ul = m_sqrtf(v3_dot(u, u)), vl = m_sqrtf(v3_dot(v, v));
        f32 us = 1.0f / (ul > 0.0f ? ul : 1.0f), vs = 1.0f / (vl > 0.0f ? vl : 1.0f);
        sum_off[c] = v3_add(sum_off[c], v3_add(v3_scale(u, us), v3_scale(v, vs)));
        hits[c] += 2.0f;
        cnx[c] = v3_add(cnx[c], m->nrm.items[i0]);
        ncnt[c] += 1.0f;
      }
    }
    f32 *curve = ALLOC_ARRAY(al, f32, clusters);
    f32 *spread = ALLOC_ARRAY(al, f32, clusters);
    for (u32 c = 0; c < clusters; c++) {
      f32 nl = m_sqrtf(v3_dot(cnx[c], cnx[c]));
      spread[c] = m_clampf(1.0f - nl / m_maxf(1.0f, ncnt[c]), 0.0f, 1.0f);
      if (nl > 1e-6f && hits[c] > 0.0f)
        curve[c] = v3_dot(cnx[c], sum_off[c]) / (nl * hits[c]);
    }

    for (u32 i = 0; i < count; i++) {
      u32 c = cluster[i];
      f32 mean = curve[c];
      f32 crease = m_minf(1.0f, spread[c] / 0.18f);
      f32 convex = crease * m_clampf((-mean - o.edge_threshold) / 0.22f, 0.0f, 1.0f);
      f32 concave = crease * m_clampf((mean - o.edge_threshold) / 0.22f, 0.0f, 1.0f);
      f32 ny = m->nrm.items[i].y;
      f32 up = m_maxf(0.0f, ny), down = m_maxf(0.0f, -ny);
      f32 w = convex * o.wear + up * up * o.up_wear * o.wear;
      f32 g = concave * o.grime + down * o.down_grime * o.grime;
      f32 a = concave * o.ao;
      if (rng) {
        f32 j = 0.85f + cod_rng_float(rng) * 0.3f;
        w *= j;
        g *= 2.0f - j;
      }
      m->col.items[i] = (v4){m_minf(1.0f, w), m_minf(1.0f, g), m_minf(1.0f, a), 1.0f};
    }
  }
}
