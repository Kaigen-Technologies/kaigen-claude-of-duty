// Claude of Duty world prop library: every prop is one merged local-space geometry.

#define CW_PB_BEGIN(A, out)                                                                                                \
  CwGeo out = {0};                                                                                                         \
  tctx_temp_allocator_scope(ta, allocator_as_arena(&(A)->al))

typedef struct CwTmp {
  Allocator *al;
} CwTmp;

// ================================================================ containers ==

hz_internal CwGeo cw_prop_crate(CwA *A, CodRng *rng, f32 s, b32 slats) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 4096, 12288);
    cw_pb_box(&p, s, s * 0.85f, s * 0.92f, 0, 0, 0, (CwPBO){.bevel = 0.012f, .grime = 0.12f});
    if (slats) {
      for (u32 i = 0; i < 3; i++) {
        f32 y = -s * 0.32f + ((f32)i / 2.0f) * s * 0.64f;
        b32 loose = cod_rng_f64(rng) < 0.18;
        f32 rz = loose ? cod_rng_range(rng, -0.12f, 0.12f) : 0.0f;
        cw_pb_box(&p, s * 1.01f, s * 0.14f, 0.016f, 0, y, s * 0.46f, (CwPBO){.bevel = 0.004f, .rz = rz, .wear = 1});
        cw_pb_box(&p, s * 1.01f, s * 0.14f, 0.016f, 0, y, -s * 0.46f, (CwPBO){.bevel = 0.004f});
        cw_pb_box(&p, 0.016f, s * 0.14f, s * 0.94f, s * 0.5f, y, 0, (CwPBO){.bevel = 0.004f});
        cw_pb_box(&p, 0.016f, s * 0.14f, s * 0.94f, -s * 0.5f, y, 0, (CwPBO){.bevel = 0.004f});
      }
      for (i32 sx = -1; sx <= 1; sx += 2)
        for (i32 sz = -1; sz <= 1; sz += 2)
          cw_pb_box(&p, 0.05f, s * 0.86f, 0.05f, (f32)sx * (s * 0.48f), 0, (f32)sz * (s * 0.44f), (CwPBO){.bevel = 0.006f});
      const u32 lid = 4;
      for (u32 i = 0; i < lid; i++) {
        f32 z = -s * 0.46f + (((f32)i + 0.5f) / (f32)lid) * s * 0.92f;
        f32 rz = cod_rng_range(rng, -0.006f, 0.006f);
        cw_pb_box(&p, s * 1.0f, 0.02f, (s * 0.92f) / (f32)lid - 0.012f, 0, s * 0.425f + 0.012f, z, (CwPBO){.bevel = 0.004f, .rz = rz, .wear = 1});
      }
      cw_pb_box(&p, s * 1.02f, 0.022f, 0.055f, 0, s * 0.44f, s * 0.2f, (CwPBO){.bevel = 0.004f, .wear = 1});
    }
    out = cw_pb_build(&p, &A->al);
  }
  cw_geo_translate(&out, 0, s * 0.425f, 0);
  return out;
}

hz_internal CwGeo cw_prop_cardboard(CwA *A, CodRng *rng, f32 s) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    f32 h = s * cod_rng_range(rng, 0.6f, 0.9f);
    f32 d = s * cod_rng_range(rng, 0.8f, 1.1f);
    cw_pb_box(&p, s, h, d, 0, 0, 0, (CwPBO){.bevel = 0.006f, .grime = 0.25f});
    cw_pb_box(&p, s * 0.48f, 0.012f, s * 0.9f, -s * 0.25f, h / 2 + 0.006f, 0, (CwPBO){.bevel = 0.003f, .wear = 1});
    cw_pb_box(&p, s * 0.48f, 0.012f, s * 0.9f, s * 0.25f, h / 2 + 0.09f, 0, (CwPBO){.bevel = 0.003f, .rz = -0.9f});
    out = cw_pb_build(&p, &A->al);
    cw_geo_translate(&out, 0, h / 2, 0);
  }
  return out;
}

hz_internal CwGeo cw_prop_barrel(CwA *A, CodRng *rng, f32 r, f32 h, u32 ribs) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 4096, 12288);
    cw_pb_cyl(&p, r, h, 0, 0, 0, (CwPBO){.radial = 16, .grime = 0.15f});
    for (u32 i = 0; i < ribs; i++) {
      f32 y = -h / 2 + (((f32)i + 1.0f) / ((f32)ribs + 1.0f)) * h;
      cw_pb_cyl(&p, r * 1.045f, h * 0.055f, 0, y, 0, (CwPBO){.radial = 16, .wear = 1, .grime = 0.3f});
    }
    cw_pb_cyl(&p, r * 1.02f, 0.03f, 0, h / 2 - 0.015f, 0, (CwPBO){.radial = 16, .wear = 1});
    cw_pb_cyl(&p, r * 1.02f, 0.03f, 0, -h / 2 + 0.015f, 0, (CwPBO){.radial = 16, .wear = 1, .grime = 0.5f});
    cw_pb_cyl(&p, 0.05f, 0.02f, r * 0.45f, h / 2 + 0.008f, 0, (CwPBO){.radial = 8, .wear = 1});
    out = cw_pb_build(&p, &A->al);
  }
  cw_geo_translate(&out, 0, h / 2, 0);
  f32 seed = (f32)(cod_rng_f64(rng) * 10.0);
  cw_warp(&out, 0.008f, 2.2f, seed);
  return out;
}

hz_internal CwGeo cw_prop_gas_bottle(CwA *A) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 2048, 6144);
    f32 h = 0.58f;
    cw_pb_cyl(&p, 0.155f, h, 0, 0, 0, (CwPBO){.radial = 14, .grime = 0.2f});
    cw_pb_cyl(&p, 0.15f, 0.06f, 0, h / 2 + 0.02f, 0, (CwPBO){.radial = 14, .taper = 0.75f, .wear = 1});
    cw_pb_cyl(&p, 0.032f, 0.09f, 0, h / 2 + 0.09f, 0, (CwPBO){.radial = 8, .wear = 1});
    cw_pb_cyl(&p, 0.075f, 0.035f, 0, h / 2 + 0.14f, 0, (CwPBO){.radial = 10, .wear = 1});
    cw_pb_cyl(&p, 0.16f, 0.02f, 0, -h / 2 + 0.01f, 0, (CwPBO){.radial = 14, .grime = 0.6f});
    out = cw_pb_build(&p, &A->al);
    cw_geo_translate(&out, 0, h / 2, 0);
  }
  return out;
}

hz_internal CwGeo cw_prop_bucket(CwA *A) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    cw_pb_cyl(&p, 0.145f, 0.28f, 0, 0, 0, (CwPBO){.radial = 14, .taper = 1.24f, .grime = 0.4f, .open = true});
    cw_pb_cyl(&p, 0.145f, 0.02f, 0, -0.13f, 0, (CwPBO){.radial = 14, .grime = 0.6f});
    cw_pb_cyl(&p, 0.185f, 0.018f, 0, 0.14f, 0, (CwPBO){.radial = 14, .wear = 1});
    out = cw_pb_build(&p, &A->al);
    cw_geo_translate(&out, 0, 0.14f, 0);
  }
  return out;
}

hz_internal CwGeo cw_prop_jerry(CwA *A) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    cw_pb_box(&p, 0.34f, 0.44f, 0.17f, 0, 0, 0, (CwPBO){.bevel = 0.02f, .grime = 0.2f});
    cw_pb_box(&p, 0.3f, 0.06f, 0.05f, 0, 0.24f, 0, (CwPBO){.bevel = 0.01f, .wear = 1});
    cw_pb_cyl(&p, 0.035f, 0.05f, 0.11f, 0.25f, 0, (CwPBO){.radial = 8, .wear = 1});
    out = cw_pb_build(&p, &A->al);
    cw_geo_translate(&out, 0, 0.22f, 0);
  }
  return out;
}

// ===================================================================== cover ==

typedef struct CwSandCtx {
  f32 min_y, dim_x, dim_y;
} CwSandCtx;

hz_internal void cw_sand_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  const CwSandCtx *c = ctx;
  f32 nn = cw_fbm3(p.x * 12, p.y * 12, p.z * 12, 2);
  f32 crease = m_maxf(0.0f, 1.0f - m_fabsf(n.y) * 3.2f);
  f32 low = m_maxf(0.0f, 1.0f - (p.y - c->min_y) / (c->dim_y * 0.55f));
  f32 end = m_maxf(0.0f, m_fabsf(p.x) / (c->dim_x * 0.5f) - 0.62f) / 0.38f;
  out[0] = 0.3f + nn * 0.45f + m_maxf(0.0f, n.y) * 0.2f;
  out[1] = 0.16f + m_maxf(0.0f, -n.y) * 0.45f + nn * 0.14f + low * low * 0.3f + end * 0.25f;
  out[2] = 0.1f + m_maxf(0.0f, -n.y) * 0.45f + crease * 0.22f + low * low * 0.35f + end * end * 0.5f;
}

hz_internal CwGeo cw_prop_sandbag(CwA *A, CodRng *rng, u32 i) {
  f32 dx = i % 3 == 0 ? 0.49f : i % 3 == 1 ? 0.45f : 0.47f;
  f32 dy = i % 3 == 0 ? 0.175f : i % 3 == 1 ? 0.16f : 0.15f;
  f32 dz = i % 3 == 0 ? 0.33f : i % 3 == 1 ? 0.35f : 0.3f;
  f32 seed = (f32)(cod_rng_f64(rng) * 50.0);
  CwGeo g = cw_sack(&A->al, seed, dx, dy, dz, i % 3, 4.6f - (f32)(i % 3) * 0.5f, 1.2f);
  aabb bb = cw_geo_bounds(&g);
  CwSandCtx c = {.min_y = bb.min.y, .dim_x = dx, .dim_y = dy};
  for (u32 k = 0; k < g.nv; k++)
    g.col[k] = V3_ZERO;
  cw_geo_paint(&g, cw_sand_paint, &c);
  cw_geo_translate(&g, 0, dy * 0.5f, 0);
  return g;
}

typedef struct CwJerseyCtx {
  f32 unused;
} CwJerseyCtx;

hz_internal void cw_jersey_paint(const void *ctx, v3 p, v3 n, f32 *o) {
  (void)ctx;
  f32 a = m_maxf(0.0f, 1.0f - p.y / 0.35f);
  f32 b = m_maxf(0.0f, 1.0f - p.y / 0.3f);
  o[1] = m_minf(1.0f, o[1] + a * a * 0.6f + m_maxf(0.0f, -n.y) * 0.4f);
  o[2] = m_minf(1.0f, o[2] + b * b * 0.45f);
}

hz_internal CwGeo cw_prop_jersey(CwA *A) {
  static const v2 prof[10] = {{-0.3f, 0},    {0.3f, 0},     {0.3f, 0.09f},   {0.16f, 0.24f}, {0.09f, 0.72f},
                              {0.09f, 0.92f}, {-0.09f, 0.92f}, {-0.09f, 0.72f}, {-0.16f, 0.24f}, {-0.3f, 0.09f}};
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 4096, 12288);
    CwGeo g = cw_extrude_poly(&ta.allocator, prof, 10, 1.9f, 0.015f);
    cw_geo_translate(&g, 0, 0, -0.95f);
    cw_geo_normals(&g);
    cw_auto_edge_wear(&g, 0.035f, 1.0f);
    cw_pb_geo(&p, &g, 0, 0, 0, (CwPBO){.no_auto_wear = true, .grime = 0.15f});
    cw_pb_cyl(&p, 0.035f, 0.1f, 0, 0.95f, -0.55f, (CwPBO){.radial = 8, .rx = CW_PI_F / 2, .wear = 1});
    cw_pb_cyl(&p, 0.035f, 0.1f, 0, 0.95f, 0.55f, (CwPBO){.radial = 8, .rx = CW_PI_F / 2, .wear = 1});
    out = cw_pb_build(&p, &A->al);
  }
  cw_geo_paint(&out, cw_jersey_paint, NULL);
  return out;
}

hz_internal CwGeo cw_prop_block(CwA *A, CodRng *rng, f32 w, f32 h, f32 d) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 2048, 6144);
    cw_pb_box(&p, w, h, d, 0, 0, 0, (CwPBO){.bevel = 0.03f, .grime = 0.2f});
    f32 seed = (f32)(cod_rng_f64(rng) * 40.0);
    CwGeo chip = cw_rock(&ta.allocator, 0.34f, 0, 0.8f, seed);
    cw_pb_geo(&p, &chip, w / 2 - 0.06f, h / 2 - 0.05f, d / 2 - 0.08f, (CwPBO){.grime = 0.4f});
    out = cw_pb_build(&p, &A->al);
    cw_geo_translate(&out, 0, h / 2, 0);
  }
  return out;
}

typedef struct CwTyreCtx {
  f32 r;
} CwTyreCtx;

hz_internal void cw_tyre_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  f32 r = ((const CwTyreCtx *)ctx)->r;
  f32 rr = m_sqrtf(p.x * p.x + p.z * p.z);
  f32 crown = m_minf(1.0f, m_maxf(0.0f, (rr / r - 0.88f) / 0.12f));
  f32 hole = m_maxf(0.0f, 1.0f - (rr / r - 0.5f) / 0.12f);
  f32 nn = cw_fbm3(p.x * 9, p.y * 9, p.z * 9, 2);
  out[0] = 0.25f + crown * 0.4f + nn * 0.25f;
  out[1] = 0.3f + (1.0f - crown) * 0.35f + m_maxf(0.0f, -n.y) * 0.3f;
  out[2] = 0.12f + (1.0f - crown) * 0.25f + m_maxf(0.0f, -n.y) * 0.3f + hole * 0.5f;
}

hz_internal CwGeo cw_prop_tyre(CwA *A, CodRng *rng, f32 r) {
  const u32 BLOCKS = 17;
  u32 radial = BLOCKS * 5;
  f32 HW = r * 0.3f;
  static const f32 pr[15] = {0.52f, 0.66f, 0.82f, 0.94f, 0.995f, 1.0f, 1.0f, 0.995f, 0.94f, 0.82f, 0.66f, 0.52f, 0.5f, 0.505f, 0.52f};
  static const f32 py[15] = {0.45f, 0.88f, 1.0f, 0.92f, 0.62f, 0.35f, -0.35f, -0.62f, -0.92f, -1.0f, -0.88f, -0.45f, -0.18f, 0.18f, 0.45f};
  CwGeo g;
  tctx_temp_allocator_scope(ta, allocator_as_arena(&A->al)) {
    v2 *prof = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v2, 15);
    for (u32 i = 0; i < 15; i++)
      prof[i] = (v2){pr[i] * r, py[i] * HW};
    g = cw_lathe(&A->al, prof, 15, radial);
  }
  f32 stagger = cod_rng_float(rng) * 6.28f;
  for (u32 i = 0; i < g.nv; i++) {
    f32 x = g.pos[i].x, y = g.pos[i].y, z = g.pos[i].z;
    f32 a = m_atan2f(z, x);
    f32 rr = m_sqrtf(x * x + z * z);
    f32 ph = (a * (f32)BLOCKS) / (CW_PI_F * 2.0f) + stagger;
    f32 blkT = (f32)(m_fmod(m_fmod((f64)ph, 1.0) + 1.0, 1.0));
    f32 blk = m_maxf(0.0f, m_minf(1.0f, m_minf(blkT / 0.075f, (0.62f - blkT) / 0.075f)));
    f32 cy = y / (HW * 0.22f);
    f32 centre = m_expf(-(cy * cy) * 3.0f);
    f32 treadBand = m_maxf(0.0f, (rr / r - 0.9f) / 0.1f) * m_maxf(0.0f, 1.0f - m_fabsf(y) / (HW * 0.72f));
    f32 grow = treadBand * (blk * 0.0062f - 0.0018f - centre * 0.0045f) * (r / 0.33f);
    f32 f = 1.0f + grow / m_maxf(1e-4f, rr);
    f32 bq = (rr / r - 0.76f) / 0.11f;
    f32 band = m_expf(-(bq * bq));
    f32 letter = band * ((m_sinf(a * 23.0f + stagger * 3.0f) > 0.4f ? 0.006f : 0.0f) + 0.0022f) * (r / 0.33f);
    g.pos[i] = (v3){x * f, y * 0.94f + cw_sign(y) * letter, z * f};
  }
  cw_geo_normals(&g);
  CwTyreCtx c = {.r = r};
  cw_geo_paint(&g, cw_tyre_paint, &c);
  cw_geo_translate(&g, 0, HW * 0.95f, 0);
  return g;
}

hz_internal CwGeo cw_prop_pallet(CwA *A, CodRng *rng) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 4096, 12288);
    f32 w = 1.16f, d = 0.98f;
    for (u32 i = 0; i < 3; i++) {
      f32 z = -d / 2 + 0.06f + ((f32)i / 2.0f) * (d - 0.12f);
      cw_pb_box(&p, w, 0.075f, 0.11f, 0, 0.04f, z, (CwPBO){.bevel = 0.006f, .grime = 0.3f});
    }
    const u32 boards = 6;
    for (u32 i = 0; i < boards; i++) {
      f32 z = -d / 2 + 0.05f + ((f32)i / (f32)(boards - 1)) * (d - 0.1f);
      f32 rz = cod_rng_range(rng, -0.004f, 0.004f);
      cw_pb_box(&p, w, 0.018f, 0.1f, 0, 0.088f, z, (CwPBO){.bevel = 0.004f, .rz = rz});
    }
    for (u32 i = 0; i < 3; i++) {
      f32 z = -d / 2 + 0.06f + ((f32)i / 2.0f) * (d - 0.12f);
      cw_pb_box(&p, w, 0.018f, 0.1f, 0, -0.008f, z, (CwPBO){.bevel = 0.004f});
    }
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

// =================================================================== furniture ==

hz_internal CwGeo cw_prop_table(CwA *A, f32 w, f32 h, f32 d) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    cw_pb_box(&p, w, 0.045f, d, 0, h - 0.02f, 0, (CwPBO){.bevel = 0.008f, .wear = 1});
    cw_pb_box(&p, w - 0.1f, 0.05f, d - 0.1f, 0, h - 0.075f, 0, (CwPBO){.bevel = 0.006f, .grime = 0.3f});
    for (i32 sx = -1; sx <= 1; sx += 2)
      for (i32 sz = -1; sz <= 1; sz += 2)
        cw_pb_box(&p, 0.07f, h - 0.1f, 0.07f, (f32)sx * (w / 2 - 0.09f), (h - 0.1f) / 2, (f32)sz * (d / 2 - 0.09f), (CwPBO){.bevel = 0.005f, .grime = 0.25f});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal CwGeo cw_prop_stall(CwA *A, f32 w) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 2048, 6144);
    f32 h = 0.84f, d = 1.05f;
    cw_pb_box(&p, w, 0.05f, d, 0, h, 0, (CwPBO){.bevel = 0.008f});
    cw_pb_box(&p, w - 0.06f, 0.09f, d - 0.08f, 0, h - 0.07f, 0, (CwPBO){.bevel = 0.006f, .grime = 0.35f});
    for (i32 sx = -1; sx <= 1; sx += 2) {
      f32 fx = (f32)sx;
      cw_pb_box(&p, 0.08f, h - 0.05f, 0.08f, fx * (w / 2 - 0.1f), (h - 0.05f) / 2, d / 2 - 0.1f, (CwPBO){.grime = 0.3f});
      cw_pb_box(&p, 0.08f, h - 0.05f, 0.08f, fx * (w / 2 - 0.1f), (h - 0.05f) / 2, -d / 2 + 0.1f, (CwPBO){.grime = 0.3f});
      cw_pb_box(&p, 0.06f, 2.0f, 0.06f, fx * (w / 2 - 0.05f), 1.0f, -d / 2 + 0.06f, (CwPBO){.grime = 0.2f});
      cw_pb_box(&p, 0.06f, 2.0f, 0.06f, fx * (w / 2 - 0.05f), 1.0f, d / 2 - 0.06f, (CwPBO){.grime = 0.2f});
    }
    cw_pb_box(&p, w, 0.06f, 0.06f, 0, 1.98f, -d / 2 + 0.06f, (CwPBO){0});
    cw_pb_box(&p, w, 0.06f, 0.06f, 0, 1.98f, d / 2 - 0.06f, (CwPBO){0});
    cw_pb_box(&p, w - 0.3f, 0.03f, d - 0.3f, 0, 0.24f, 0, (CwPBO){.bevel = 0.004f, .grime = 0.45f});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal CwGeo cw_prop_shelf(CwA *A) {
  f32 w = 1.1f, h = 1.9f, d = 0.35f;
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 2048, 6144);
    for (i32 sx = -1; sx <= 1; sx += 2)
      cw_pb_box(&p, 0.05f, h, d, (f32)sx * (w / 2 - 0.025f), h / 2, 0, (CwPBO){.grime = 0.2f});
    const u32 n = 4;
    for (u32 i = 0; i < n; i++) {
      f32 y = 0.22f + ((f32)i / (f32)(n - 1)) * (h - 0.4f);
      cw_pb_box(&p, w - 0.06f, 0.03f, d, 0, y, 0, (CwPBO){.bevel = 0.005f, .grime = 0.25f});
    }
    cw_pb_box(&p, w, 0.03f, 0.02f, 0, h - 0.02f, -d / 2 + 0.01f, (CwPBO){0});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal void cw_mattress_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)ctx;
  f32 nn = cw_fbm3(p.x * 3 + 4, p.y * 3, p.z * 3, 2);
  out[0] = 0.2f;
  out[1] = 0.45f + nn * 0.4f;
  out[2] = m_maxf(0.0f, -n.y) * 0.4f;
}

hz_internal CwGeo cw_prop_mattress(CwA *A) {
  CwGeo g = cw_chamfer_box(&A->al, 1.85f, 0.16f, 0.85f, 0.05f);
  cw_geo_paint(&g, cw_mattress_paint, NULL);
  for (u32 i = 0; i < g.nv; i++) {
    v3 p = g.pos[i];
    if (p.y > 0.0f)
      g.pos[i].y = p.y - 0.035f * m_cosf((p.x / 1.85f) * CW_PI_F) * m_cosf((p.z / 0.85f) * CW_PI_F);
  }
  cw_geo_normals(&g);
  cw_geo_translate(&g, 0, 0.08f, 0);
  return g;
}

hz_internal CwGeo cw_prop_chair(CwA *A) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    f32 sh = 0.46f;
    cw_pb_box(&p, 0.42f, 0.04f, 0.4f, 0, sh, 0, (CwPBO){.bevel = 0.006f, .wear = 1});
    for (i32 sx = -1; sx <= 1; sx += 2)
      for (i32 sz = -1; sz <= 1; sz += 2)
        cw_pb_box(&p, 0.04f, sh, 0.04f, (f32)sx * 0.18f, sh / 2, (f32)sz * 0.17f, (CwPBO){.grime = 0.2f});
    cw_pb_box(&p, 0.42f, 0.5f, 0.035f, 0, sh + 0.27f, -0.18f, (CwPBO){.bevel = 0.005f, .rx = -0.08f});
    cw_pb_box(&p, 0.42f, 0.06f, 0.05f, 0, sh + 0.48f, -0.2f, (CwPBO){.bevel = 0.005f});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal CwGeo cw_prop_cabinet(CwA *A) {
  f32 w = 0.9f, h = 1.15f, d = 0.44f;
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    cw_pb_box(&p, w, h, d, 0, h / 2, 0, (CwPBO){.bevel = 0.01f, .grime = 0.2f});
    for (i32 sx = -1; sx <= 1; sx += 2) {
      f32 fx = (f32)sx;
      cw_pb_box(&p, w / 2 - 0.03f, h - 0.12f, 0.03f, fx * (w / 4), h / 2, d / 2 + 0.01f, (CwPBO){.bevel = 0.005f, .wear = 1});
      cw_pb_box(&p, 0.03f, 0.1f, 0.03f, fx * 0.06f, h / 2, d / 2 + 0.03f, (CwPBO){.wear = 1});
    }
    cw_pb_box(&p, w + 0.04f, 0.04f, d + 0.04f, 0, h + 0.02f, 0, (CwPBO){.bevel = 0.008f, .wear = 1, .grime = 0.3f});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

// ================================================================== services ==

hz_internal CwGeo cw_prop_ac(CwA *A) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 3072, 9216);
    f32 w = 0.78f, h = 0.55f, d = 0.34f;
    cw_pb_box(&p, w, h, d, 0, 0, 0, (CwPBO){.bevel = 0.012f, .grime = 0.35f});
    for (u32 i = 0; i < 7; i++)
      cw_pb_box(&p, w - 0.1f, 0.035f, 0.02f, 0, -h / 2 + 0.08f + (f32)i * 0.06f, d / 2 + 0.005f, (CwPBO){.bevel = 0.003f, .rx = 0.35f, .wear = 1});
    cw_pb_cyl(&p, 0.19f, 0.03f, 0, 0.02f, d / 2 + 0.02f, (CwPBO){.radial = 16, .rx = CW_PI_F / 2, .wear = 1});
    for (i32 sx = -1; sx <= 1; sx += 2) {
      f32 fx = (f32)sx;
      cw_pb_box(&p, 0.05f, 0.05f, 0.5f, fx * (w / 2 - 0.05f), -h / 2 + 0.03f, -d / 2 - 0.16f, (CwPBO){.grime = 0.5f});
      cw_pb_box(&p, 0.05f, 0.34f, 0.05f, fx * (w / 2 - 0.05f), -h / 2 - 0.14f, -d / 2 - 0.36f, (CwPBO){.grime = 0.5f, .rz = 0.5f});
    }
    cw_pb_cyl(&p, 0.012f, 0.5f, w / 2 - 0.12f, -h / 2 - 0.24f, 0, (CwPBO){.radial = 6, .grime = 0.6f});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal CwGeo cw_prop_sat_dish(CwA *A) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 2048, 6144);
    CwGeo dish = cw_sphere(&ta.allocator, 0.42f, 16, 10, 0.55f);
    cw_geo_apply(&dish, cw_trs(0, 0, 0, 0, 1, 0.42f, 1, 0, 0));
    cw_geo_apply(&dish, cw_trs(0, 0, 0, 0, 1, 1, 1, -2.1f, 0));
    cw_auto_edge_wear(&dish, 0.03f, 0.8f);
    cw_pb_geo(&p, &dish, 0, 0.55f, 0.1f, (CwPBO){.no_auto_wear = true, .grime = 0.3f});
    cw_pb_cyl(&p, 0.03f, 0.5f, 0, 0.4f, -0.12f, (CwPBO){.radial = 8, .rx = 0.5f, .wear = 1});
    cw_pb_cyl(&p, 0.045f, 0.55f, 0, 0.27f, -0.22f, (CwPBO){.radial = 8, .grime = 0.4f});
    cw_pb_box(&p, 0.24f, 0.03f, 0.24f, 0, 0.02f, -0.22f, (CwPBO){.bevel = 0.005f, .grime = 0.6f});
    cw_pb_cyl(&p, 0.028f, 0.16f, 0, 0.62f, 0.34f, (CwPBO){.radial = 6, .rx = 1.1f, .wear = 1});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal CwGeo cw_prop_water_tank(CwA *A) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 3072, 9216);
    cw_pb_cyl(&p, 0.55f, 1.0f, 0, 0.5f, 0, (CwPBO){.radial = 18, .grime = 0.3f});
    cw_pb_cyl(&p, 0.56f, 0.05f, 0, 0.99f, 0, (CwPBO){.radial = 18, .wear = 1});
    cw_pb_cyl(&p, 0.18f, 0.09f, 0.16f, 1.05f, 0, (CwPBO){.radial = 12, .wear = 1});
    cw_pb_cyl(&p, 0.03f, 0.5f, -0.5f, 0.2f, 0, (CwPBO){.radial = 6, .grime = 0.5f, .rz = 0.3f});
    for (i32 sz = -1; sz <= 1; sz += 2)
      cw_pb_box(&p, 1.2f, 0.09f, 0.09f, 0, 0.045f, (f32)sz * 0.36f, (CwPBO){.grime = 0.5f});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal CwGeo cw_prop_roof_vent(CwA *A) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    cw_pb_box(&p, 0.5f, 0.3f, 0.5f, 0, 0.15f, 0, (CwPBO){.bevel = 0.01f, .grime = 0.4f});
    cw_pb_cyl(&p, 0.17f, 0.36f, 0, 0.48f, 0, (CwPBO){.radial = 12, .grime = 0.3f});
    cw_pb_cyl(&p, 0.24f, 0.06f, 0, 0.68f, 0, (CwPBO){.radial = 12, .wear = 1});
    cw_pb_cyl(&p, 0.2f, 0.05f, 0, 0.74f, 0, (CwPBO){.radial = 12, .taper = 0.3f, .wear = 1});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal CwGeo cw_prop_street_lamp(CwA *A, f32 h) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 3072, 9216);
    cw_pb_cyl(&p, 0.13f, 0.35f, 0, 0.17f, 0, (CwPBO){.radial = 12, .grime = 0.6f});
    cw_pb_cyl(&p, 0.075f, h, 0, h / 2, 0, (CwPBO){.radial = 10, .taper = 0.7f, .grime = 0.25f});
    const u32 segs = 5;
    for (u32 i = 0; i < segs; i++) {
      f32 t = (f32)i / (f32)(segs - 1);
      f32 a = t * 1.35f;
      cw_pb_cyl(&p, 0.055f, 0.44f, m_sinf(a) * 0.62f * (0.4f + t), h - 0.1f + m_cosf(a) * 0.34f * t, 0, (CwPBO){.radial = 8, .rz = -a, .grime = 0.3f});
    }
    cw_pb_cyl(&p, 0.028f, 0.95f, 0.32f, h - 0.42f, 0, (CwPBO){.radial = 6, .rz = -0.72f, .grime = 0.4f});
    cw_pb_box(&p, 0.1f, 0.16f, 0.1f, 0.05f, h - 0.72f, 0, (CwPBO){.bevel = 0.01f, .grime = 0.45f});
    cw_pb_box(&p, 0.5f, 0.13f, 0.28f, 0.86f, h + 0.06f, 0, (CwPBO){.bevel = 0.02f, .rz = -0.16f, .grime = 0.35f});
    cw_pb_box(&p, 0.42f, 0.06f, 0.22f, 0.88f, h - 0.02f, 0, (CwPBO){.bevel = 0.01f, .rz = -0.16f, .wear = 1});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal CwGeo cw_prop_lamp_glass(CwA *A) {
  CwGeo g = cw_chamfer_box(&A->al, 0.4f, 0.05f, 0.2f, 0.01f);
  cw_geo_fill_masks(&g, 0.2f, 0.1f, 0.0f);
  return g;
}

// ==================================================================== debris ==

hz_internal void cw_brick_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)ctx;
  out[0] = 0.5f + cw_fbm3(p.x * 9, p.y * 9, p.z * 9, 2) * 0.5f;
  out[1] = 0.4f + m_maxf(0.0f, -n.y) * 0.4f;
  out[2] = 0.25f;
}

hz_internal CwGeo cw_prop_brick(CwA *A, CodRng *rng) {
  f32 seed = (f32)(cod_rng_f64(rng) * 40.0);
  CwGeo g = cw_rock(&A->al, 0.22f, 0, 0.55f, seed);
  cw_geo_paint(&g, cw_brick_paint, NULL);
  return g;
}

hz_internal CwGeo cw_prop_slab_shard(CwA *A, CodRng *rng) {
  f32 w = cod_rng_range(rng, 0.5f, 0.95f);
  f32 d = cod_rng_range(rng, 0.35f, 0.7f);
  v2 pts[7];
  for (u32 i = 0; i < 7; i++) {
    f32 t = ((f32)i / 7.0f) * (f32)CW_TAU;
    f32 rr = 0.5f * (0.6f + cw_fbm3(m_cosf(t) * 3 + 2, m_sinf(t) * 3, 5, 2) * 0.8f);
    pts[i] = (v2){m_cosf(t) * rr * w, m_sinf(t) * rr * d};
  }
  f32 height = cod_rng_range(rng, 0.07f, 0.13f);
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    CwGeo g = cw_poly_prism(&ta.allocator, pts, 7, height);
    cw_auto_edge_wear(&g, 0.02f, 1.0f);
    cw_pb_geo(&p, &g, 0, 0, 0, (CwPBO){.no_auto_wear = true, .grime = 0.4f});
    i32 bars = cod_rng_int(rng, 2, 4);
    for (i32 i = 0; i < bars; i++) {
      f32 a = cod_rng_float(rng) * (f32)CW_TAU;
      f32 len = cod_rng_range(rng, 0.3f, 0.7f);
      f32 rz = cod_rng_range(rng, -1.4f, 1.4f);
      f32 rx = cod_rng_range(rng, -1.2f, 1.2f);
      cw_pb_cyl(&p, 0.008f, len, m_cosf(a) * w * 0.3f, 0.06f, m_sinf(a) * d * 0.3f, (CwPBO){.radial = 5, .rz = rz, .rx = rx, .grime = 0.5f});
    }
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal CwGeo cw_prop_rebar(CwA *A, CodRng *rng) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    i32 n = cod_rng_int(rng, 4, 7);
    for (i32 i = 0; i < n; i++) {
      f32 len = cod_rng_range(rng, 1.4f, 2.6f);
      f32 x = cod_rng_range(rng, -0.08f, 0.08f);
      f32 z = cod_rng_range(rng, -0.06f, 0.06f);
      f32 ry = cod_rng_range(rng, -0.12f, 0.12f);
      cw_pb_cyl(&p, 0.009f, len, x, 0.012f + (f32)i * 0.019f, z, (CwPBO){.radial = 5, .rx = CW_PI_F / 2, .ry = ry, .grime = 0.55f});
    }
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal void cw_plank_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)ctx;
  (void)p;
  out[1] = m_minf(1.0f, out[1] + 0.3f + m_maxf(0.0f, -n.y) * 0.4f);
}

hz_internal CwGeo cw_prop_plank(CwA *A, CodRng *rng) {
  f32 sx = cod_rng_range(rng, 0.9f, 2.1f);
  f32 sz = cod_rng_range(rng, 0.12f, 0.2f);
  CwGeo g = cw_chamfer_box(&A->al, sx, 0.035f, sz, 0.005f);
  cw_auto_edge_wear(&g, 0.012f, 1.0f);
  f32 seed = (f32)(cod_rng_f64(rng) * 9.0);
  cw_warp(&g, 0.012f, 1.4f, seed);
  cw_geo_paint(&g, cw_plank_paint, NULL);
  return g;
}

// the swept fillet of dust and grit under standing objects: unit radius, 2.5 cm proud, ragged rim
hz_internal CwGeo cw_prop_dust_skirt(CwA *A) {
  CwGeo g = cw_cylinder(&A->al, 1.0f, 1.0f, 0.001f, 26, 4, false);
  g.has_col = true;
  for (u32 i = 0; i < g.nv; i++) {
    f32 x = g.pos[i].x, z = g.pos[i].z;
    f32 d = m_minf(1.0f, m_sqrtf(x * x + z * z));
    f32 a = m_atan2f(z, x);
    f32 wob = 0.86f + 0.28f * cw_fbm3(m_cosf(a) * 2.2f, m_sinf(a) * 2.2f, 3.1, 3);
    f32 dd = d * wob;
    f32 t = m_maxf(0.0f, 1.0f - dd);
    f32 y = t * t * 0.021f + (cw_fbm3(x * 6, z * 6, 9.4, 3) - 0.5f) * 0.004f * (1.0f - dd);
    g.pos[i] = (v3){x * wob, y, z * wob};
    g.col[i] = (v3){0.05f, 0.35f + 0.6f * t, 0.3f + 0.55f * t};
  }
  cw_geo_normals(&g);
  return g;
}

hz_internal CwGeo cw_prop_litter(CwA *A, CodRng *rng) {
  f32 w = cod_rng_range(rng, 0.1f, 0.22f);
  f32 h = cod_rng_range(rng, 0.1f, 0.28f);
  CwGeo g = cw_plane(&A->al, w, h, 2, 2);
  for (u32 i = 0; i < g.nv; i++)
    g.pos[i].z = (cw_fbm3(g.pos[i].x * 20, g.pos[i].y * 20, 3, 2) - 0.5f) * 0.035f;
  cw_geo_apply(&g, cw_trs(0, 0, 0, 0, 1, 1, 1, -CW_PI_F / 2, 0));
  cw_geo_normals(&g);
  cw_geo_fill_masks(&g, 0.3f, 0.5f, 0.2f);
  return g;
}

hz_internal CwGeo cw_prop_bottle(CwA *A) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    cw_pb_cyl(&p, 0.038f, 0.17f, 0, 0.085f, 0, (CwPBO){.radial = 10, .grime = 0.3f});
    cw_pb_cyl(&p, 0.02f, 0.08f, 0, 0.2f, 0, (CwPBO){.radial = 8, .taper = 0.8f});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal CwGeo cw_prop_can(CwA *A) {
  CwGeo g = cw_cylinder(&A->al, 0.033f, 0.033f, 0.115f, 10, 1, false);
  cw_auto_edge_wear(&g, 0.01f, 1.0f);
  for (u32 i = 0; i < g.nv; i++) {
    f32 y = g.pos[i].y;
    g.pos[i].x *= 1.0f - m_fabsf(y) * 1.2f;
  }
  cw_geo_normals(&g);
  cw_geo_apply(&g, cw_trs(0, 0, 0, 0, 1, 1, 1, 0, 1.4f));
  cw_geo_translate(&g, 0, 0.033f, 0);
  return g;
}

// ================================================================ vegetation ==

hz_internal CwGeo cw_prop_palm_trunk(CwA *A, CodRng *rng, f32 h) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 4096, 12288);
    u32 segs = 9;
    f32 lean = cod_rng_range(rng, -0.1f, 0.1f);
    for (u32 i = 0; i < segs; i++) {
      f32 t = (f32)i / (f32)segs;
      f32 r = 0.19f * (1.0f - t * 0.42f);
      f32 y = t * h;
      f32 x = m_sinf(t * 2.2f + lean * 4.0f) * lean * h * 0.4f;
      cw_pb_cyl(&p, r, h / (f32)segs + 0.02f, x, y + h / (f32)segs / 2, 0, (CwPBO){.radial = 9, .taper = 0.92f, .grime = 0.3f + t * 0.2f, .wear = 1});
      cw_pb_cyl(&p, r * 1.13f, 0.045f, x, y + h / (f32)segs * 0.75f, 0, (CwPBO){.radial = 9, .wear = 1, .grime = 0.4f});
    }
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

// one palm frond: leaflets along a curved spine, foliage-textured quads
hz_internal CwGeo cw_prop_palm_frond(CwA *A, f32 len) {
  const u32 n = 13;
  CwGeo out = cw_geo_new(&A->al, (n * 2 + 1) * 4 + 14, (n * 2 + 1) * 6 + 36);
  out.has_col = true;
  tctx_temp_allocator_scope(ta, allocator_as_arena(&A->al)) {
    for (u32 i = 0; i < n; i++) {
      f32 t = ((f32)i + 1.0f) / ((f32)n + 1.0f);
      f32 x = t * len;
      f32 droop = -t * t * len * 0.42f;
      f32 lw = (0.42f + m_sinf(t * CW_PI_F) * 0.55f) * (1.0f - t * 0.35f);
      for (i32 side = -1; side <= 1; side += 2) {
        CwGeo q = cw_plane(&ta.allocator, lw, 0.16f, 1, 1);
        cw_geo_translate(&q, lw / 2, 0, 0);
        cw_geo_apply(&q, cw_trs(0, 0, 0, 0, 1, 1, 1, 0, -0.5f - t * 0.5f));
        cw_geo_apply(&q, cw_trs(0, 0, 0, (f32)side * (1.15f - t * 0.35f), 1, 1, 1, 0, 0));
        cw_geo_apply(&q, cw_trs(x, droop, 0, 0, 1, 1, 1, 0, 0));
        cw_geo_fill_masks(&q, 0.2f, 0.25f, 0.0f);
        cw_geo_append(&out, &q);
      }
    }
    CwGeo spine = cw_plane(&ta.allocator, len, 0.05f, 6, 1);
    for (u32 i = 0; i < spine.nv; i++) {
      v3 p = spine.pos[i];
      f32 x = p.x + len / 2;
      spine.pos[i] = (v3){x, p.y - (x / len) * (x / len) * len * 0.42f, p.z};
    }
    cw_geo_normals(&spine);
    cw_geo_fill_masks(&spine, 0.2f, 0.3f, 0.0f);
    cw_geo_append(&out, &spine);
  }
  return out;
}

hz_internal CwGeo cw_prop_shrub(CwA *A, CodRng *rng, f32 s) {
  const u32 n = 7;
  CwGeo out = cw_geo_new(&A->al, n * 4, n * 6);
  out.has_col = true;
  tctx_temp_allocator_scope(ta, allocator_as_arena(&A->al)) {
    for (u32 i = 0; i < n; i++) {
      f32 w = s * cod_rng_range(rng, 0.7f, 1.15f);
      f32 h = s * cod_rng_range(rng, 0.6f, 1.0f);
      CwGeo q = cw_plane(&ta.allocator, w, h, 1, 1);
      f32 mx = cod_rng_range(rng, -s * 0.2f, s * 0.2f);
      f32 my = s * cod_rng_range(rng, 0.28f, 0.6f);
      f32 mz = cod_rng_range(rng, -s * 0.2f, s * 0.2f);
      f32 ry = cod_rng_float(rng) * CW_PI_F;
      f32 rx = cod_rng_range(rng, -0.4f, 0.4f);
      f32 rz = cod_rng_range(rng, -0.3f, 0.3f);
      cw_geo_apply(&q, cw_trs(mx, my, mz, ry, 1, 1, 1, rx, rz));
      cw_geo_fill_masks(&q, 0.2f, 0.35f, 0.2f);
      cw_geo_append(&out, &q);
    }
  }
  return out;
}

hz_internal CwGeo cw_prop_weeds(CwA *A, CodRng *rng) {
  const u32 n = 4;
  CwGeo out = cw_geo_new(&A->al, n * 4, n * 6);
  out.has_col = true;
  tctx_temp_allocator_scope(ta, allocator_as_arena(&A->al)) {
    for (u32 i = 0; i < n; i++) {
      f32 w = cod_rng_range(rng, 0.18f, 0.34f);
      f32 h = cod_rng_range(rng, 0.14f, 0.3f);
      CwGeo q = cw_plane(&ta.allocator, w, h, 1, 1);
      f32 mx = cod_rng_range(rng, -0.06f, 0.06f);
      f32 my = cod_rng_range(rng, 0.07f, 0.17f);
      f32 mz = cod_rng_range(rng, -0.06f, 0.06f);
      f32 ry = cod_rng_float(rng) * 3.14f;
      f32 rx = cod_rng_range(rng, -0.5f, 0.5f);
      cw_geo_apply(&q, cw_trs(mx, my, mz, ry, 1, 1, 1, rx, 0));
      cw_geo_fill_masks(&q, 0.2f, 0.5f, 0.3f);
      cw_geo_append(&out, &q);
    }
  }
  return out;
}

hz_internal CwGeo cw_prop_planter(CwA *A) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    cw_pb_cyl(&p, 0.34f, 0.42f, 0, 0.21f, 0, (CwPBO){.radial = 14, .taper = 0.78f, .grime = 0.4f});
    cw_pb_cyl(&p, 0.36f, 0.05f, 0, 0.42f, 0, (CwPBO){.radial = 14, .wear = 1});
    cw_pb_cyl(&p, 0.3f, 0.06f, 0, 0.4f, 0, (CwPBO){.radial = 12, .grime = 0.9f});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

// =================================================================== signage ==

hz_internal CwGeo cw_prop_sign_board(CwA *A, f32 w, f32 h) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    cw_pb_box(&p, w, h, 0.05f, 0, 0, 0, (CwPBO){.bevel = 0.008f, .grime = 0.25f});
    cw_pb_box(&p, w + 0.05f, 0.045f, 0.07f, 0, h / 2, 0, (CwPBO){.bevel = 0.006f, .wear = 1});
    cw_pb_box(&p, w + 0.05f, 0.045f, 0.07f, 0, -h / 2, 0, (CwPBO){.bevel = 0.006f, .wear = 1});
    for (i32 sx = -1; sx <= 1; sx += 2)
      cw_pb_box(&p, 0.03f, 0.24f, 0.12f, (f32)sx * (w / 2 - 0.12f), 0, -0.08f, (CwPBO){.grime = 0.5f});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

hz_internal CwGeo cw_prop_sign_hang(CwA *A, f32 w, f32 h) {
  CW_PB_BEGIN(A, out) {
    CwPB p = cw_pb_begin(&ta.allocator, 1024, 3072);
    cw_pb_box(&p, w, h, 0.04f, 0, -h / 2 - 0.12f, 0, (CwPBO){.bevel = 0.006f, .grime = 0.3f});
    cw_pb_cyl(&p, 0.014f, 0.14f, -w / 2 + 0.08f, -0.06f, 0, (CwPBO){.radial = 6, .wear = 1});
    cw_pb_cyl(&p, 0.014f, 0.14f, w / 2 - 0.08f, -0.06f, 0, (CwPBO){.radial = 6, .wear = 1});
    cw_pb_cyl(&p, 0.018f, w + 0.14f, 0, 0, 0, (CwPBO){.radial = 6, .rz = CW_PI_F / 2, .wear = 1, .grime = 0.4f});
    out = cw_pb_build(&p, &A->al);
  }
  return out;
}

// ================================================================== vehicles ==

hz_internal void cw_car_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)ctx;
  f32 soot = 0.45f + 0.5f * m_maxf(0.0f, n.y) + 0.3f * m_maxf(0.0f, 1.0f - m_fabsf(p.z) / 1.6f);
  out[1] = m_minf(1.0f, out[1] + soot * 0.8f);
  out[0] = m_minf(1.0f, out[0] * 0.8f);
}

// a burnt-out saloon: sagging roof, blown glass, missing wheels
hz_internal CwGeo cw_prop_burnt_car(CwA *A) {
  CW_PB_BEGIN(A, out) {
    CwPB b = cw_pb_begin(&ta.allocator, 8192, 24576);
    f32 L = 4.35f, W = 1.78f;
    cw_pb_box(&b, W, 0.5f, L, 0, 0.62f, 0, (CwPBO){.bevel = 0.05f, .grime = 0.5f});
    cw_pb_box(&b, W * 0.99f, 0.34f, L * 0.62f, 0, 0.95f, -0.15f, (CwPBO){.bevel = 0.06f, .grime = 0.5f});
    cw_pb_box(&b, W * 0.94f, 0.13f, L * 0.3f, 0, 0.94f, L * 0.33f, (CwPBO){.bevel = 0.03f, .rx = 0.06f, .wear = 1});
    cw_pb_box(&b, W * 0.94f, 0.13f, L * 0.22f, 0, 0.95f, -L * 0.38f, (CwPBO){.bevel = 0.03f, .rx = -0.08f, .wear = 1});
    f32 rh = 1.42f;
    for (i32 sx = -1; sx <= 1; sx += 2) {
      f32 fx = (f32)sx;
      cw_pb_box(&b, 0.09f, 0.55f, 0.1f, fx * (W / 2 - 0.08f), 1.2f, L * 0.14f, (CwPBO){.rx = 0.35f, .grime = 0.4f});
      cw_pb_box(&b, 0.09f, 0.5f, 0.1f, fx * (W / 2 - 0.08f), 1.22f, -L * 0.02f, (CwPBO){.grime = 0.4f});
      cw_pb_box(&b, 0.11f, 0.52f, 0.12f, fx * (W / 2 - 0.08f), 1.2f, -L * 0.2f, (CwPBO){.rx = -0.3f, .grime = 0.4f});
      cw_pb_box(&b, 0.07f, 0.42f, L * 0.42f, fx * (W / 2 - 0.03f), 0.68f, 0.05f, (CwPBO){.bevel = 0.02f, .wear = 1, .grime = 0.5f});
    }
    cw_pb_box(&b, W * 0.86f, 0.07f, L * 0.36f, 0, rh - 0.04f, -L * 0.04f, (CwPBO){.bevel = 0.04f, .wear = 1, .grime = 0.6f});
    for (i32 sx = -1; sx <= 1; sx += 2)
      for (i32 sz = -1; sz <= 1; sz += 2)
        cw_pb_cyl(&b, 0.42f, 0.1f, (f32)sx * (W / 2 - 0.04f), 0.5f, (f32)sz * L * 0.31f, (CwPBO){.radial = 12, .rz = CW_PI_F / 2, .open = true, .grime = 0.5f});
    cw_pb_box(&b, W * 0.98f, 0.22f, 0.16f, 0, 0.5f, L / 2 - 0.05f, (CwPBO){.bevel = 0.03f, .wear = 1, .grime = 0.5f});
    cw_pb_box(&b, W * 0.98f, 0.22f, 0.16f, 0, 0.5f, -L / 2 + 0.05f, (CwPBO){.bevel = 0.03f, .wear = 1, .grime = 0.5f, .rz = 0.05f});
    out = cw_pb_build(&b, &A->al);
  }
  cw_geo_paint(&out, cw_car_paint, NULL);
  return out;
}

// ============================================================= dressing props ==

hz_internal CwGeo cw_prop_wheel_flat(CwA *A) {
  CwGeo g = cw_torus(&A->al, 0.24f, 0.11f, 10, 16);
  cw_geo_apply(&g, cw_trs(0, 0, 0, CW_PI_F / 2, 1, 1, 1, 0, 0));
  for (u32 i = 0; i < g.nv; i++)
    g.pos[i].y *= 0.82f;
  cw_geo_normals(&g);
  cw_geo_fill_masks(&g, 0.3f, 0.6f, 0.2f);
  return g;
}

hz_internal CwGeo cw_prop_glass_shards(CwA *A, CodRng *rng) {
  CwGeo out = cw_geo_new(&A->al, 9 * 96, 9 * 132);
  out.has_col = true;
  tctx_temp_allocator_scope(ta, allocator_as_arena(&A->al)) {
    for (u32 i = 0; i < 9; i++) {
      f32 s = 0.03f + cod_rng_float(rng) * 0.06f;
      f32 sz = s * cod_rng_range(rng, 0.5f, 1.6f);
      CwGeo g = cw_chamfer_box(&ta.allocator, s, 0.004f, sz, 0.001f);
      f32 ry = cod_rng_float(rng) * 6.28f;
      f32 px = cod_rng_range(rng, -0.5f, 0.5f);
      f32 pz = cod_rng_range(rng, -0.4f, 0.4f);
      cw_geo_apply(&g, cw_trs(px, 0.003f, pz, ry, 1, 1, 1, 0, 0));
      cw_geo_fill_masks(&g, 0.6f, 0.2f, 0.0f);
      cw_geo_append(&out, &g);
    }
  }
  return out;
}

hz_internal void cw_cinder_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)ctx;
  (void)p;
  out[0] = 0.7f;
  out[1] = 0.3f + m_maxf(0.0f, -n.y) * 0.5f;
  out[2] = m_maxf(0.0f, -n.y) * 0.4f;
}

hz_internal CwGeo cw_prop_cinder(CwA *A) {
  CwGeo g = cw_chamfer_box(&A->al, 0.44f, 0.21f, 0.21f, 0.012f);
  cw_geo_paint(&g, cw_cinder_paint, NULL);
  cw_geo_translate(&g, 0, 0.105f, 0);
  return g;
}

hz_internal void cw_tray_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)ctx;
  (void)p;
  (void)n;
  out[0] = 0.8f;
  out[1] = 0.35f;
}

hz_internal CwGeo cw_prop_tray(CwA *A) {
  CwGeo out = cw_geo_new(&A->al, 5 * 96, 5 * 132);
  out.has_col = true;
  tctx_temp_allocator_scope(ta, allocator_as_arena(&A->al)) {
    for (u32 k = 0; k < 5; k++) {
      f32 sx = 0.6f, sy = 0.02f, sz = 0.42f, x = 0, y = 0.01f, z = 0;
      if (k > 0) {
        f32 s = (k & 1) ? -1.0f : 1.0f;
        if (k <= 2) {
          sx = 0.6f; sy = 0.09f; sz = 0.02f; y = 0.055f; z = s * 0.2f;
        } else {
          sx = 0.02f; sy = 0.09f; sz = 0.42f; y = 0.055f; x = s * 0.29f;
        }
      }
      CwGeo g = cw_chamfer_box(&ta.allocator, sx, sy, sz, 0.005f);
      cw_geo_translate(&g, x, y, z);
      cw_geo_append(&out, &g);
    }
  }
  cw_geo_paint(&out, cw_tray_paint, NULL);
  return out;
}

hz_internal CwGeo cw_prop_produce(CwA *A, CodRng *rng) {
  CwGeo out = cw_geo_new(&A->al, 7 * 64, 7 * 192);
  out.has_col = true;
  tctx_temp_allocator_scope(ta, allocator_as_arena(&A->al)) {
    for (u32 i = 0; i < 7; i++) {
      f32 size = cod_rng_range(rng, 0.055f, 0.1f);
      f32 seed = (f32)(cod_rng_f64(rng) * 40.0);
      CwGeo g = cw_rock(&ta.allocator, size, 0, 0.8f, seed);
      f32 tx = cod_rng_range(rng, -0.22f, 0.22f);
      f32 ty = 0.035f + cod_rng_range(rng, 0.0f, 0.04f);
      f32 tz = cod_rng_range(rng, -0.14f, 0.14f);
      cw_geo_translate(&g, tx, ty, tz);
      cw_geo_append(&out, &g);
    }
  }
  cw_geo_fill_masks(&out, 0.15f, 0.2f, 0.1f);
  return out;
}

hz_internal void cw_conduit_box_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)ctx;
  (void)p;
  (void)n;
  out[0] = 0.85f;
  out[1] = 0.45f;
}

hz_internal CwGeo cw_prop_conduit_box(CwA *A) {
  CwGeo out = cw_geo_new(&A->al, 2 * 96, 2 * 132);
  out.has_col = true;
  tctx_temp_allocator_scope(ta, allocator_as_arena(&A->al)) {
    CwGeo b = cw_chamfer_box(&ta.allocator, 0.2f, 0.26f, 0.11f, 0.008f);
    cw_geo_append(&out, &b);
    CwGeo lid = cw_chamfer_box(&ta.allocator, 0.17f, 0.22f, 0.02f, 0.004f);
    cw_geo_translate(&lid, 0, 0, 0.065f);
    cw_geo_append(&out, &lid);
  }
  cw_geo_paint(&out, cw_conduit_box_paint, NULL);
  return out;
}

hz_internal void cw_stool_paint(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)ctx;
  (void)p;
  out[0] = 0.8f;
  out[1] = 0.3f + m_maxf(0.0f, -n.y) * 0.4f;
}

hz_internal CwGeo cw_prop_stool(CwA *A) {
  CwGeo out = cw_geo_new(&A->al, 5 * 96, 5 * 132);
  out.has_col = true;
  tctx_temp_allocator_scope(ta, allocator_as_arena(&A->al)) {
    CwGeo top = cw_chamfer_box(&ta.allocator, 0.34f, 0.04f, 0.34f, 0.01f);
    cw_geo_translate(&top, 0, 0.42f, 0);
    cw_geo_append(&out, &top);
    for (i32 sx = -1; sx <= 1; sx += 2)
      for (i32 sz = -1; sz <= 1; sz += 2) {
        CwGeo leg = cw_chamfer_box(&ta.allocator, 0.035f, 0.42f, 0.035f, 0.005f);
        cw_geo_apply(&leg, cw_trs((f32)sx * 0.13f, 0.21f, (f32)sz * 0.13f, 0, 1, 1, 1, 0, (f32)sx * 0.06f));
        cw_geo_append(&out, &leg);
      }
  }
  cw_geo_paint(&out, cw_stool_paint, NULL);
  return out;
}

// =================================================================== registry ==

#define CW_LOOSE(t, s) .tilt = (t), .sink = (s)

// the order is the RNG order
hz_internal void cw_register_props(CwA *A, CodRng *rng) {
  CwProtoOpts z = {0};
  (void)z;
  cwa_proto(A, CWP_CRATE_A, cw_prop_crate(A, rng, 0.64f, true), CW_WOOD_PROP, (CwProtoOpts){.skirt = 0.37f, CW_LOOSE(0.09f, 0.022f)});
  cwa_proto(A, CWP_CRATE_B, cw_prop_crate(A, rng, 0.48f, true), CW_WOOD_PROP, (CwProtoOpts){CW_LOOSE(0.10f, 0.018f)});
  cwa_proto(A, CWP_CRATE_C, cw_prop_crate(A, rng, 0.82f, true), CW_WOOD_PROP_DARK, (CwProtoOpts){.skirt = 0.45f, CW_LOOSE(0.075f, 0.026f)});
  cwa_proto(A, CWP_CRATE_FLAT, cw_prop_crate(A, rng, 0.55f, false), CW_WOOD_PROP, (CwProtoOpts){CW_LOOSE(0.10f, 0.02f)});
  cwa_proto(A, CWP_BOX_CARD_A, cw_prop_cardboard(A, rng, 0.46f), CW_WOOD_PALE, (CwProtoOpts){CW_LOOSE(0.10f, 0.016f)});
  cwa_proto(A, CWP_BOX_CARD_B, cw_prop_cardboard(A, rng, 0.34f), CW_WOOD_PALE, (CwProtoOpts){CW_LOOSE(0.11f, 0.012f)});
  cwa_proto(A, CWP_BARREL_RUST, cw_prop_barrel(A, rng, 0.29f, 0.88f, 3), CW_METAL_RUST_PROP, (CwProtoOpts){.skirt = 0.28f, CW_LOOSE(0.085f, 0.014f)});
  cwa_proto(A, CWP_BARREL_BLUE, cw_prop_barrel(A, rng, 0.28f, 0.9f, 2), CW_METAL_BLUE, (CwProtoOpts){.skirt = 0.26f, CW_LOOSE(0.085f, 0.014f)});
  cwa_proto(A, CWP_BARREL_WOOD, cw_prop_barrel(A, rng, 0.31f, 0.78f, 4), CW_WOOD_PROP_DARK, (CwProtoOpts){.skirt = 0.28f, CW_LOOSE(0.09f, 0.015f)});
  cwa_proto(A, CWP_GAS_BOTTLE, cw_prop_gas_bottle(A), CW_METAL_GREEN, (CwProtoOpts){.skirt = 0.18f, CW_LOOSE(0.07f, 0.008f)});
  cwa_proto(A, CWP_BUCKET, cw_prop_bucket(A), CW_METAL_RUST_PROP, (CwProtoOpts){CW_LOOSE(0.12f, 0.008f)});
  cwa_proto(A, CWP_JERRY_CAN, cw_prop_jerry(A), CW_METAL_GREEN, (CwProtoOpts){CW_LOOSE(0.10f, 0.01f)});

  cwa_proto(A, CWP_SANDBAG_A, cw_prop_sandbag(A, rng, 0), CW_BURLAP, (CwProtoOpts){CW_LOOSE(0.085f, 0.006f)});
  cwa_proto(A, CWP_SANDBAG_B, cw_prop_sandbag(A, rng, 1), CW_BURLAP, (CwProtoOpts){CW_LOOSE(0.09f, 0.006f)});
  cwa_proto(A, CWP_SANDBAG_C, cw_prop_sandbag(A, rng, 2), CW_BURLAP, (CwProtoOpts){CW_LOOSE(0.095f, 0.006f)});
  cwa_proto(A, CWP_JERSEY, cw_prop_jersey(A), CW_CONCRETE_PROP, (CwProtoOpts){.skirt = 0.69f});
  cwa_proto(A, CWP_BLOCK_BIG, cw_prop_block(A, rng, 1.25f, 0.95f, 0.85f), CW_CONCRETE_PROP, (CwProtoOpts){.skirt = 0.63f, CW_LOOSE(0.05f, 0.03f)});
  cwa_proto(A, CWP_BLOCK_SMALL, cw_prop_block(A, rng, 0.55f, 0.42f, 0.4f), CW_CONCRETE_DARK, (CwProtoOpts){.skirt = 0.31f, CW_LOOSE(0.09f, 0.018f)});
  cwa_proto(A, CWP_TYRE, cw_prop_tyre(A, rng, 0.33f), CW_RUBBER, (CwProtoOpts){.skirt = 0.33f, CW_LOOSE(0.10f, 0.008f)});
  cwa_proto(A, CWP_TYRE_SMALL, cw_prop_tyre(A, rng, 0.26f), CW_RUBBER, (CwProtoOpts){CW_LOOSE(0.11f, 0.006f)});
  cwa_proto(A, CWP_PALLET, cw_prop_pallet(A, rng), CW_WOOD_PROP, (CwProtoOpts){.skirt = 0.51f, CW_LOOSE(0.055f, 0.02f)});

  cwa_proto(A, CWP_TABLE, cw_prop_table(A, 1.5f, 0.78f, 0.8f), CW_WOOD_PROP_DARK, (CwProtoOpts){.skirt = 0.57f});
  cwa_proto(A, CWP_TABLE_SMALL, cw_prop_table(A, 0.9f, 0.72f, 0.7f), CW_WOOD_PROP, (CwProtoOpts){0});
  cwa_proto(A, CWP_STALL, cw_prop_stall(A, 2.3f), CW_WOOD_PROP_DARK, (CwProtoOpts){.skirt = 0.90f});
  cwa_proto(A, CWP_SHELF, cw_prop_shelf(A), CW_WOOD_PROP_DARK, (CwProtoOpts){.skirt = 0.42f});
  cwa_proto(A, CWP_MATTRESS, cw_prop_mattress(A), CW_FABRIC_CREAM, (CwProtoOpts){CW_LOOSE(0.06f, 0.01f)});
  cwa_proto(A, CWP_CHAIR, cw_prop_chair(A), CW_WOOD_PROP, (CwProtoOpts){CW_LOOSE(0.05f, 0.012f)});
  cwa_proto(A, CWP_CABINET, cw_prop_cabinet(A), CW_WOOD_PROP_DARK, (CwProtoOpts){.skirt = 0.42f});

  cwa_proto(A, CWP_AC_UNIT, cw_prop_ac(A), CW_METAL_DARK, (CwProtoOpts){0});
  cwa_proto(A, CWP_SAT_DISH, cw_prop_sat_dish(A), CW_METAL_DARK, (CwProtoOpts){0});
  cwa_proto(A, CWP_WATER_TANK, cw_prop_water_tank(A), CW_METAL_BLUE, (CwProtoOpts){.skirt = 0.48f});
  cwa_proto(A, CWP_ROOF_VENT, cw_prop_roof_vent(A), CW_METAL_RUST, (CwProtoOpts){0});
  cwa_proto(A, CWP_LAMP_POST, cw_prop_street_lamp(A, 5.4f), CW_METAL_DARK, (CwProtoOpts){.skirt = 0.25f});
  cwa_proto(A, CWP_LAMP_GLASS, cw_prop_lamp_glass(A), CW_LAMP_LENS, (CwProtoOpts){.no_shadow = true});

  cwa_proto(A, CWP_BRICK_A, cw_prop_brick(A, rng), CW_BRICK, (CwProtoOpts){CW_LOOSE(0.16f, 0.006f)});
  cwa_proto(A, CWP_BRICK_B, cw_prop_brick(A, rng), CW_BRICK, (CwProtoOpts){CW_LOOSE(0.16f, 0.006f)});
  f32 seed_a = (f32)(cod_rng_f64(rng) * 40.0);
  cwa_proto(A, CWP_ROCK_A, cw_rock(&A->al, 0.26f, 0, 0.7f, seed_a), CW_CONCRETE_PROP, (CwProtoOpts){.max_dist = 90});
  f32 seed_b = (f32)(cod_rng_f64(rng) * 40.0);
  cwa_proto(A, CWP_ROCK_B, cw_rock(&A->al, 0.17f, 0, 0.8f, seed_b), CW_CONCRETE_DARK, (CwProtoOpts){.max_dist = 70, .no_shadow = true});
  cwa_proto(A, CWP_SLAB_SHARD, cw_prop_slab_shard(A, rng), CW_CONCRETE_PROP, (CwProtoOpts){CW_LOOSE(0.14f, 0.01f)});
  cwa_proto(A, CWP_REBAR, cw_prop_rebar(A, rng), CW_METAL_RUST, (CwProtoOpts){CW_LOOSE(0.10f, 0.004f)});
  cwa_proto(A, CWP_PLANK_A, cw_prop_plank(A, rng), CW_WOOD_PROP, (CwProtoOpts){.max_dist = 90, CW_LOOSE(0.06f, 0.004f)});
  cwa_proto(A, CWP_PLANK_B, cw_prop_plank(A, rng), CW_WOOD_PROP_DARK, (CwProtoOpts){.max_dist = 90, CW_LOOSE(0.06f, 0.004f)});
  cwa_proto(A, CWP_LITTER, cw_prop_litter(A, rng), CW_WOOD_PALE, (CwProtoOpts){.max_dist = 45, .no_shadow = true});
  cwa_proto(A, CWP_DUST_SKIRT, cw_prop_dust_skirt(A), CW_DUST_SKIRT, (CwProtoOpts){.max_dist = 42, .no_shadow = true});
  cwa_proto(A, CWP_BOTTLE, cw_prop_bottle(A), CW_GLASS, (CwProtoOpts){.max_dist = 55, .no_shadow = true});
  cwa_proto(A, CWP_CAN, cw_prop_can(A), CW_STEEL, (CwProtoOpts){.max_dist = 45, .no_shadow = true});

  cwa_proto(A, CWP_PALM_TRUNK, cw_prop_palm_trunk(A, rng, 5.4f), CW_WOOD_DARK, (CwProtoOpts){.skirt = 0.57f});
  cwa_proto(A, CWP_PALM_FROND, cw_prop_palm_frond(A, 2.7f), CW_FOLIAGE, (CwProtoOpts){0});
  cwa_proto(A, CWP_SHRUB, cw_prop_shrub(A, rng, 0.85f), CW_FOLIAGE, (CwProtoOpts){0});
  cwa_proto(A, CWP_WEEDS, cw_prop_weeds(A, rng), CW_FOLIAGE, (CwProtoOpts){.max_dist = 40});
  cwa_proto(A, CWP_PLANTER, cw_prop_planter(A), CW_CONCRETE_PROP, (CwProtoOpts){.skirt = 0.33f, CW_LOOSE(0.07f, 0.014f)});

  cwa_proto(A, CWP_SIGN_BOARD, cw_prop_sign_board(A, 1.6f, 0.55f), CW_METAL_BLUE, (CwProtoOpts){.skirt = 0.18f});
  cwa_proto(A, CWP_SIGN_HANG, cw_prop_sign_hang(A, 0.9f, 0.62f), CW_METAL_GREEN, (CwProtoOpts){0});

  cwa_proto(A, CWP_POCK, cw_pock(&A->al, rng, 0.032f), CW_CONCRETE_DARK, (CwProtoOpts){.max_dist = 65, .no_shadow = true});

  // dressing props
  cwa_proto(A, CWP_WRECK, cw_prop_burnt_car(A), CW_METAL_DARK, (CwProtoOpts){0});
  cwa_proto(A, CWP_WHEEL_FLAT, cw_prop_wheel_flat(A), CW_RUBBER, (CwProtoOpts){0});
  cwa_proto(A, CWP_GLASS_SHARDS, cw_prop_glass_shards(A, rng), CW_GLASS, (CwProtoOpts){.max_dist = 40, .no_shadow = true});
  cwa_proto(A, CWP_CINDER, cw_prop_cinder(A), CW_CONCRETE_PROP, (CwProtoOpts){0});
  cwa_proto(A, CWP_TRAY, cw_prop_tray(A), CW_WOOD_PROP, (CwProtoOpts){0});
  cwa_proto(A, CWP_PRODUCE, cw_prop_produce(A, rng), CW_BURLAP, (CwProtoOpts){.max_dist = 60});
  cwa_proto(A, CWP_CONDUIT_BOX, cw_prop_conduit_box(A), CW_METAL_DARK, (CwProtoOpts){.max_dist = 55});
  cwa_proto(A, CWP_STOOL, cw_prop_stool(A), CW_WOOD_PROP, (CwProtoOpts){0});
}
