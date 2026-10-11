// Claude of Duty world ground: terrain, cambered road, pavement slabs, alleys, material seams, drifts and covers.

#define CW_TERRAIN_S 168.0f
#define CW_TERRAIN_N 42u

typedef struct CwSeamCtx {
  CodRng *sr;
} CwSeamCtx;

hz_internal CwGeoId cwa_terrain(CwA *A) {
  CwSpec sp = {.kind = CWS_TERRAIN};
  return cwa_spec(A, sp);
}

hz_internal CwGeoId cwa_road(CwA *A) {
  CwSpec sp = {.kind = CWS_ROAD};
  return cwa_spec(A, sp);
}

hz_internal void cw_terrain_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)ctx;
  (void)n;
  out[1] = 0.25f + cw_fbm3(p.x * 0.3f, 1.1, p.z * 0.3f, 2) * 0.4f;
  out[0] = 0.2f;
}

// the sandy ground under everything, gently undulating, with the street flat
hz_internal CwGeo cw_make_terrain(Allocator *al) {
  CwGeo g = cw_plane(al, CW_TERRAIN_S, CW_TERRAIN_S, CW_TERRAIN_N, CW_TERRAIN_N);
  cw_geo_apply(&g, cw_trs(0, 0, 0, 0, 1, 1, 1, -CW_PI_F / 2, 0));
  for (u32 i = 0; i < g.nv; i++) {
    f32 x = g.pos[i].x, z = g.pos[i].z;
    b32 in_street = m_fabsf(x) < CW_STREET_KERB + 1 && z > CW_STREET_ZMIN && z < CW_STREET_ZMAX;
    f32 h = in_street ? 0.0f : (cw_fbm3(x * 0.045f, 7.3, z * 0.045f, 3) - 0.5f) * 1.1f + 0.02f;
    g.pos[i].y = h - 0.03f;
  }
  cw_geo_normals(&g);
  for (u32 i = 0; i < g.nv; i++)
    g.col[i] = V3_ZERO;
  cw_geo_paint(&g, cw_terrain_paint, NULL);
  return g;
}

hz_internal void cw_road_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)ctx;
  (void)n;
  f32 nn = cw_fbm3(p.x * 0.7f, 4.4, p.z * 0.7f, 3);
  out[1] = 0.1f + m_maxf(0.0f, (m_fabsf(p.x) - 3.4f) / 1.5f) * 0.35f + nn * 0.18f;
  out[0] = 0.2f + nn * 0.3f;
}

hz_internal CwGeo cw_make_road(Allocator *al) {
  f32 HW = CW_STREET_HW;
  f32 road_len = CW_STREET_ZMAX - CW_STREET_ZMIN;
  CwGeo g = cw_plane(al, HW * 2, road_len, 12, (u32)m_roundf(road_len / 2));
  cw_geo_apply(&g, cw_trs(0, 0, 0, 0, 1, 1, 1, -CW_PI_F / 2, 0));
  for (u32 i = 0; i < g.nv; i++) {
    f32 x = g.pos[i].x, z = g.pos[i].z;
    f32 camber = (1.0f - (x / HW) * (x / HW)) * 0.055f;
    f32 wear = (cw_fbm3(x * 0.55f + 3, 2.2, z * 0.35f, 3) - 0.5f) * 0.07f;
    f32 ax = m_fabsf(x) - 1.6f;
    f32 rut = -m_expf(-(ax * ax) / 0.5f) * 0.022f;
    g.pos[i].y = camber + wear + rut;
  }
  cw_geo_normals(&g);
  for (u32 i = 0; i < g.nv; i++)
    g.col[i] = V3_ZERO;
  cw_geo_paint(&g, cw_road_paint, NULL);
  cw_geo_translate(&g, 0, 0, (CW_STREET_ZMIN + CW_STREET_ZMAX) / 2);
  return g;
}

// seam: interlocked patches of both materials straddling a boundary, plus loose stones; sr is its own fixed stream
hz_internal void cw_ground_seam(CwA *A, CodRng *sr, f32 ax, f32 az, f32 bx, f32 bz, u32 keyA, u32 keyB, f32 y) {
  f32 len = m_sqrtf((bx - ax) * (bx - ax) + (bz - az) * (bz - az));
  i32 n = (i32)m_maxf(6.0f, m_roundf(len / 1.15f));
  f32 tx = (bx - ax) / len, tz = (bz - az) / len;
  f32 nxs = -tz, nzs = tx;
  for (i32 i = 0; i < n; i++) {
    f32 t = (((f32)i + cod_rng_range(sr, 0.15f, 0.85f)) / (f32)n) * len;
    f32 px = ax + tx * t, pz = az + tz * t;
    for (i32 k = 0; k < 2; k++) {
      u32 key = k == 0 ? keyA : keyB;
      f32 side = k == 0 ? -1.0f : 1.0f;
      if (cod_rng_f64(sr) < 0.22)
        continue;
      f32 off = side * cod_rng_range(sr, -0.12f, 0.62f);
      f32 prad = cod_rng_range(sr, 0.3f, 0.62f);
      CwGeoId g = cwa_patch(A, sr, prad, 10, 0.6f);
      f32 yo = cod_rng_range(sr, 0.0f, 0.004f);
      f32 ry = cod_rng_float(sr) * CW_TAU32;
      f32 sz = cod_rng_range(sr, 0.55f, 1.0f);
      f32 mg = cod_rng_range(sr, 0.3f, 0.8f);
      f32 ma = cod_rng_range(sr, 0.2f, 0.5f);
      cwa_add(A, key, g, LL(IDENT, px + nxs * off, y + 0.006f + yo, pz + nzs * off, ry, 1, 1, sz), CW_M(0.15f, mg, ma));
    }
    if (cwa_has(A, CWP_ROCK_B)) {
      for (i32 k = 0; k < cod_rng_int(sr, 1, 3); k++) {
        f32 off = cod_rng_range(sr, -0.55f, 0.55f);
        u32 id = cod_rng_f64(sr) < 0.68 ? CWP_ROCK_B : CWP_ROCK_A;
        f32 jx = cod_rng_range(sr, -0.2f, 0.2f);
        f32 jz = cod_rng_range(sr, -0.2f, 0.2f);
        f32 ry = cod_rng_float(sr) * CW_TAU32;
        f32 sc = cod_rng_range(sr, 0.45f, 1.0f);
        f32 gg = cod_rng_range(sr, 1.0f, 1.5f);
        cwa_put_g(A, id, px + nxs * off + jx, y + 0.01f, pz + nzs * off + jz, ry, sc, gg);
      }
    }
  }
}

hz_internal void cw_build_ground(CwA *A, CodRng *rng) {
  const f32 HW = CW_STREET_HW, KB = CW_STREET_KERB, WH = CW_STREET_WALK_H, zMin = CW_STREET_ZMIN, zMax = CW_STREET_ZMAX;

  // terrain, with its own real triangles as collision
  CwGeoId terrain = cwa_terrain(A);
  cwa_add(A, CW_SAND, terrain, IDENT, NULL);
  cwa_collide_geo(A, COD_SURFACE_SAND, terrain, IDENT);

  // the cambered road, collision a flat box
  CwGeoId road = cwa_road(A);
  cwa_add(A, CW_ROAD_DUST, road, IDENT, NULL);
  cwa_box(A, COD_SURFACE_DIRT, 0, -0.2f, (zMin + zMax) / 2, HW * 2, 0.42f, zMax - zMin, 0.0f);

  // old tarmac showing through the dust where wheels have polished it
  for (i32 i = 0; i < 30; i++) {
    b32 rut = cod_rng_f64(rng) < 0.62;
    f32 x;
    if (rut) {
      f32 sg = cod_rng_f64(rng) < 0.5 ? -1.0f : 1.0f;
      x = sg * cod_rng_range(rng, 1.2f, 2.1f);
    } else {
      x = cod_rng_range(rng, -HW + 0.5f, HW - 0.5f);
    }
    f32 z = cod_rng_range(rng, zMin + 2, zMax - 2);
    f32 camber = (1.0f - (x / HW) * (x / HW)) * 0.055f + 0.042f;
    f32 prad = cod_rng_range(rng, 0.45f, 1.1f);
    CwGeoId g = cwa_patch(A, rng, prad, 11, 0.5f);
    f32 ry = cod_rng_float(rng) * 0.4f;
    f32 sz = rut ? cod_rng_range(rng, 2.0f, 4.5f) : cod_rng_range(rng, 0.7f, 1.4f);
    cwa_add(A, CW_ASPHALT, g, LL(IDENT, x, camber, z, ry, 1, 1, sz), CW_M(0.35f, 0.25f, 0.1f));
  }

  // pavement slabs
  for (i32 side = -1; side <= 1; side += 2) {
    f32 z = zMin;
    while (z < zMax) {
      f32 segLen = cod_rng_range(rng, 3.2f, 6.5f);
      f32 gap = cod_rng_f64(rng) < 0.12 ? cod_rng_range(rng, 0.6f, 1.6f) : 0.06f;
      b32 mouth = false;
      for (u32 ai = 0; ai < CW_ALLEY_COUNT; ai++) {
        const CwAlley *a = &CW_ALLEYS[ai];
        b32 inX = side > 0 ? a->x0 >= KB - 0.5f : a->x1 <= -KB + 0.5f;
        if (inX && z + segLen > a->z0 - 0.2f && z < a->z1 + 0.2f)
          mouth = true;
      }
      f32 cz = z + segLen / 2;
      f32 cx = (f32)side * (KB + HW) / 2;
      f32 wSlab = KB - HW;
      if (!mouth) {
        f32 h = WH + cod_rng_range(rng, -0.012f, 0.012f);
        cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(IDENT, cx, h / 2, cz, 0, wSlab - 0.05f, h, segLen - gap), CW_M(0.6f, 0.45f, 0.2f));
        cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(IDENT, (f32)side * (HW + 0.11f), (h + 0.022f) / 2, cz, 0, 0.22f, h + 0.022f, segLen - gap), CW_M(0.95f, 0.35f, 0.1f));
        cwa_box(A, COD_SURFACE_CONCRETE, cx, h / 2, cz, wSlab, h, segLen - gap * 0.5f, 0.0f);
        cwa_box(A, COD_SURFACE_CONCRETE, (f32)side * (HW + 0.11f), (h + 0.022f) / 2, cz, 0.24f, h + 0.022f, segLen - gap * 0.5f, 0.0f);
        if (cod_rng_f64(rng) < 0.5) {
          f32 prad = cod_rng_range(rng, 0.25f, 0.7f);
          CwGeoId g = cwa_patch(A, rng, prad, 9, 0.6f);
          f32 jx = cod_rng_range(rng, -0.5f, 0.5f);
          f32 jz = cod_rng_range(rng, -1.0f, 1.0f);
          f32 ry = cod_rng_float(rng) * CW_TAU32;
          cwa_add(A, CW_CONCRETE, g, LL(IDENT, cx + jx, h + 0.006f, cz + jz, ry, 1, 1, 1), CW_M(0.1f, 1.0f, 0.55f));
        }
      } else {
        cwa_add(A, CW_DIRT, BOX(A), LL(IDENT, cx, 0.035f, cz, 0, wSlab, 0.07f, segLen), CW_M(0.2f, 0.7f, 0.4f));
        cwa_box(A, COD_SURFACE_DIRT, cx, 0.03f, cz, wSlab, 0.06f, segLen, 0.0f);
      }
      z += segLen + gap;
    }
  }

  // alleys
  for (u32 ai = 0; ai < CW_ALLEY_COUNT; ai++) {
    const CwAlley *a = &CW_ALLEYS[ai];
    f32 w = a->x1 - a->x0, d = a->z1 - a->z0;
    cwa_add(A, a->key, BOX(A), LL(IDENT, (a->x0 + a->x1) / 2, 0.03f, (a->z0 + a->z1) / 2, 0, w, 0.06f, d), CW_M(0.2f, 0.6f, 0.35f));
    cwa_box(A, cw_surface_of(a->key), (a->x0 + a->x1) / 2, 0.02f, (a->z0 + a->z1) / 2, w, 0.05f, d, 0.0f);
  }

  // material seams, from their own fixed-seed stream
  CodRng sr = cod_rng_make(0x5ea31du);
  cw_ground_seam(A, &sr, -HW + 0.08f, zMin + 2, -HW + 0.08f, zMax - 2, CW_SAND, CW_ROAD_DUST, 0.012f);
  cw_ground_seam(A, &sr, HW - 0.08f, zMin + 2, HW - 0.08f, zMax - 2, CW_SAND, CW_ROAD_DUST, 0.012f);
  cw_ground_seam(A, &sr, -KB, zMin + 2, -KB, zMax - 2, CW_CONCRETE, CW_SAND, WH + 0.004f);
  cw_ground_seam(A, &sr, KB, zMin + 2, KB, zMax - 2, CW_CONCRETE, CW_SAND, WH + 0.004f);
  for (u32 ai = 0; ai < CW_ALLEY_COUNT; ai++) {
    const CwAlley *a = &CW_ALLEYS[ai];
    f32 ay = 0.062f;
    cw_ground_seam(A, &sr, a->x0, a->z0, a->x1, a->z0, a->key, CW_SAND, ay);
    cw_ground_seam(A, &sr, a->x0, a->z1, a->x1, a->z1, a->key, CW_SAND, ay);
    cw_ground_seam(A, &sr, a->x0, a->z0, a->x0, a->z1, a->key, CW_SAND, ay);
    cw_ground_seam(A, &sr, a->x1, a->z0, a->x1, a->z1, a->key, CW_SAND, ay);
  }

  // drifts, stains and covers
  for (i32 i = 0; i < 130; i++) {
    f32 side = cod_rng_f64(rng) < 0.5 ? -1.0f : 1.0f;
    b32 againstWall = cod_rng_f64(rng) < 0.55;
    f32 x = againstWall ? side * (KB - cod_rng_range(rng, 0.05f, 0.9f)) : side * (HW + cod_rng_range(rng, -0.35f, 0.5f));
    f32 z = cod_rng_range(rng, zMin + 2, zMax - 2);
    f32 y = againstWall ? WH + 0.012f : (m_fabsf(x) < HW ? (1.0f - (x / HW) * (x / HW)) * 0.055f + 0.05f : WH + 0.01f);
    f32 prad = cod_rng_range(rng, 0.35f, 1.5f);
    CwGeoId g = cwa_patch(A, rng, prad, 9, 0.5f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sz = cod_rng_range(rng, 0.5f, 1.0f);
    cwa_add(A, CW_SAND, g, LL(IDENT, x, y, z, ry, 1, 1, sz), CW_M(0.15f, 0.5f, 0.3f));
  }
  for (i32 i = 0; i < 26; i++) {
    f32 prad = cod_rng_range(rng, 0.5f, 1.8f);
    CwGeoId g = cwa_patch(A, rng, prad, 10, 0.6f);
    f32 px = cod_rng_range(rng, -HW + 0.4f, HW - 0.4f);
    f32 pz = cod_rng_range(rng, zMin + 3, zMax - 3);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sz = cod_rng_range(rng, 0.4f, 0.9f);
    cwa_add(A, CW_DIRT, g, LL(IDENT, px, (1.0f - (px / HW) * (px / HW)) * 0.055f + 0.048f, pz, ry, 1, 1, sz), CW_M(0.1f, 0.85f, 0.5f));
  }
  // manholes and gully gratings
  for (i32 i = 0; i < 7; i++) {
    f32 z = cod_rng_range(rng, zMin + 6, zMax - 6);
    f32 x = cod_rng_range(rng, -2.5f, 2.5f);
    CwGeoId ring;
    if (!cwa_cache_find(A, CWK_MANHOLE, 0, 0, &ring)) {
      CwGeo g = cw_cylinder(&A->al, 0.36f, 0.36f, 0.04f, 18, 1, false);
      for (u32 k = 0; k < g.nv; k++)
        g.col[k] = (v3){g.nrm[k].y > 0.5f ? 0.95f : 0.4f, 0.55f, 0.0f};
      g.has_col = true;
      ring = cwa_cache_store(A, CWK_MANHOLE, 0, 0, cwa_ready(A, g));
    }
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    cwa_add(A, CW_METAL_DARK, ring, LL(IDENT, x, 0.035f + (1.0f - (x / HW) * (x / HW)) * 0.05f, z, ry, 1, 1, 1), NULL);
  }
  for (i32 side = -1; side <= 1; side += 2) {
    for (i32 i = 0; i < 5; i++) {
      f32 z = cod_rng_range(rng, zMin + 8, zMax - 8);
      cwa_add(A, CW_METAL_DARK, BOX(A), LL(IDENT, (f32)side * (HW - 0.22f), WH - 0.03f, z, 0, 0.42f, 0.05f, 0.62f), CW_M(0.7f, 0.8f, 0.6f));
    }
  }
}
