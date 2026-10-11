// Claude of Duty world interiors: furnished rooms behind the facades.

enum { CWK_CONDUIT = 20, CWK_BULBWIRE, CWK_BULB, CWK_AERIAL, CWK_MANHOLE };

typedef struct CwRoom {
  u32 kind;
  i32 street;
  f32 x0, z0, x1, z1, y, h;
} CwRoom;

hz_internal const u16 CW_L_RUBBLE3[] = {CWP_BRICK_A, CWP_BRICK_B, CWP_ROCK_B};
hz_internal const u16 CW_L_SHELF_GOODS[] = {CWP_BOTTLE, CWP_CAN, CWP_BOX_CARD_B, CWP_BUCKET};
hz_internal const u16 CW_L_BASE[] = {CWP_SANDBAG_A, CWP_SANDBAG_B, CWP_CRATE_B, CWP_BOX_CARD_A, CWP_BOX_CARD_B,
                                     CWP_BUCKET,    CWP_JERRY_CAN, CWP_TYRE_SMALL, CWP_BARREL_WOOD};
hz_internal const u16 CW_L_WEDGE[] = {CWP_BRICK_A, CWP_BRICK_B, CWP_ROCK_B, CWP_LITTER, CWP_LITTER};
hz_internal const u16 CW_L_LEAN[] = {CW_PLYWOOD, CW_CORRUGATED, CW_WOOD_PROP_DARK};
hz_internal const u16 CW_L_SHOP_COUNTER[] = {CWP_BOX_CARD_A, CWP_BOX_CARD_B, CWP_CRATE_B, CWP_BOTTLE, CWP_CAN, CWP_BUCKET};
hz_internal const u16 CW_L_SHOP_FRONT[] = {CWP_SANDBAG_A, CWP_SANDBAG_B, CWP_TRAY, CWP_CRATE_B, CWP_CRATE_FLAT};
hz_internal const u16 CW_L_SHOP_SHELF[] = {CWP_BOX_CARD_B, CWP_BOTTLE, CWP_CAN};
hz_internal const u16 CW_L_SANDBAG3[] = {CWP_SANDBAG_A, CWP_SANDBAG_B, CWP_SANDBAG_C};
hz_internal const u16 CW_L_FABRIC3[] = {CW_FABRIC_RED, CW_FABRIC_TEAL, CW_FABRIC_CREAM};
hz_internal const u16 CW_L_STORE_BARREL[] = {CWP_BARREL_RUST, CWP_BARREL_BLUE, CWP_BARREL_WOOD};
hz_internal const u16 CW_L_STORE_PALLET[] = {CWP_SANDBAG_A, CWP_SANDBAG_B, CWP_BOX_CARD_A};
hz_internal const u16 CW_L_RUIN_RUBBLE[] = {CWP_BRICK_A, CWP_BRICK_B, CWP_ROCK_A, CWP_ROCK_B};
hz_internal const u16 CW_L_CRATES4[] = {CWP_CRATE_A, CWP_CRATE_B, CWP_CRATE_C, CWP_CRATE_FLAT};

// leaning crates, each bedded on the last; returns the top y
hz_internal f32 cw_stack_crates(CwA *A, CodRng *rng, f32 x, f32 y, f32 z, i32 n) {
  f32 cy = y;
  b32 was = A->skirts;
  for (i32 i = 0; i < n; i++) {
    A->skirts = was && i == 0;
    u32 id = CW_PICK(rng, CW_L_CRATES4);
    f32 s = cod_rng_range(rng, 0.92f, 1.08f);
    f32 hh = id == CWP_CRATE_C ? 0.82f * 0.85f : id == CWP_CRATE_B ? 0.48f * 0.85f : 0.62f * 0.85f;
    f32 px = x + cod_rng_range(rng, -0.12f, 0.12f);
    f32 pz = z + cod_rng_range(rng, -0.12f, 0.12f);
    f32 ry = cod_rng_range(rng, -0.5f, 0.5f);
    f32 g = cod_rng_range(rng, 0.7f, 1.4f);
    cwa_put_g(A, id, px, cy, pz, ry, s, g);
    cwa_box(A, COD_SURFACE_WOOD, x, cy + (hh * s) / 2, z, 0.7f, hh * s, 0.7f, 0.0f);
    cy += hh * s;
    if (cod_rng_f64(rng) < 0.2)
      break;
  }
  A->skirts = was;
  return cy;
}

hz_internal void cw_add_rug(CwA *A, CodRng *rng, f32 x, f32 y, f32 z, f32 size) {
  f32 h = size * cod_rng_range(rng, 0.55f, 0.75f);
  CwClothSpec cs = cw_cloth_defaults(size, h);
  cs.seg_x = 8;
  cs.seg_y = 6;
  cs.sag = 0.0f;
  cs.wrinkle = 0.02f;
  cs.thickness = 0.0038f;
  cs.fray = 0.012f;
  cs.seed = cw_cloth_seed(rng);
  CwGeoId g = cwa_cloth(A, cs);
  u32 key = CW_PICK(rng, CW_L_FABRIC3);
  f32 ry = cod_rng_range(rng, -0.4f, 0.4f);
  cwa_add(A, key, g, LLR(IDENT, x, y + 0.014f, z, ry, 1, 1, 1, -CW_PI_F / 2, 0), CW_M(0.45f, 0.55f, 0.25f));
}

// a patch geometry draws lobes+1 floats
hz_internal CwGeoId cw_patch_geo(CwA *A, CodRng *rng, f32 radius, u32 lobes, f32 wobble) { return cwa_patch(A, rng, radius, lobes, wobble); }

hz_internal void cw_hanging_bulb(CwA *A, CodRng *rng, f32 x, f32 yCeil, f32 z) {
  f32 drop = cod_rng_range(rng, 0.35f, 0.95f);
  CwGeoId wire;
  if (!cwa_cache_find(A, CWK_BULBWIRE, 0, 0, &wire)) {
    CwGeo g = cw_cylinder(&A->al, 0.006f, 0.006f, 1.0f, 5, 1, false);
    cw_geo_fill_masks(&g, 0.2f, 0.4f, 0.0f);
    wire = cwa_cache_store(A, CWK_BULBWIRE, 0, 0, cwa_ready(A, g));
  }
  cwa_add(A, CW_METAL_DARK, wire, LL(IDENT, x, yCeil - drop / 2, z, 0, 1, drop, 1), NULL);
  cwa_add(A, CW_METAL_DARK, BOX_FINE(A), LL(IDENT, x, yCeil - 0.02f, z, 0, 0.09f, 0.04f, 0.09f), CW_M(0.5f, 0.6f, 0.3f));
  CwGeoId bulb;
  if (!cwa_cache_find(A, CWK_BULB, 0, 0, &bulb)) {
    CwGeo g = cw_sphere(&A->al, 0.045f, 10, 7, 0.0f);
    cw_geo_apply(&g, cw_trs(0, 0, 0, 0, 1, 1.25f, 1, 0, 0));
    cw_geo_fill_masks(&g, 0.1f, 0.2f, 0.0f);
    bulb = cwa_cache_store(A, CWK_BULB, 0, 0, cwa_ready(A, g));
  }
  cwa_add(A, CW_EMISSIVE_WARM, bulb, LL(IDENT, x, yCeil - drop - 0.05f, z, 0, 1, 1, 1), NULL);
  cwa_add(A, CW_METAL_DARK, BOX_FINE(A), LL(IDENT, x, yCeil - drop + 0.02f, z, 0, 0.05f, 0.06f, 0.05f), CW_M(0.6f, 0.4f, 0.0f));
  dyn_arr_append(A->interior_lights, ((v3){x, yCeil - drop - 0.05f, z}));
}

// ------------------------------------------------------------------ walls --

typedef struct CwWallSide {
  f32 px, pz, tx, tz, nx, nz, len, yaw;
} CwWallSide;

hz_internal v2 cw_wall_at(const CwWallSide *s, f32 t, f32 off) { return (v2){s->px + s->tx * t + s->nx * off, s->pz + s->tz * t + s->nz * off}; }

hz_internal f32 cw_wall_t(CodRng *rng, b32 opening, f32 half) {
  if (opening) {
    f32 sgn = cod_rng_f64(rng) < 0.5 ? -1.0f : 1.0f;
    return sgn * cod_rng_range(rng, half * 0.62f, half);
  }
  return cod_rng_range(rng, -half, half);
}

hz_internal void cw_dress_walls(CwA *A, CodRng *rng, const CwRoom *r) {
  f32 x0 = r->x0, z0 = r->z0, x1 = r->x1, z1 = r->z1, y = r->y, h = r->h;
  CwWallSide sides[4] = {
      {(x0 + x1) / 2, z0, 1, 0, 0, 1, x1 - x0, 0.0f},
      {x1, (z0 + z1) / 2, 0, 1, -1, 0, z1 - z0, CW_PI_F / 2},
      {(x0 + x1) / 2, z1, 1, 0, 0, -1, x1 - x0, 0.0f},
      {x0, (z0 + z1) / 2, 0, -1, 1, 0, z1 - z0, CW_PI_F / 2},
  };
  for (i32 side = 0; side < 4; side++) {
    const CwWallSide *s = &sides[side];
    if (s->len < 1.6f)
      continue;
    f32 half = s->len / 2 - 0.35f;
    b32 opening = side == r->street;

    // surface conduit: two drops and a run under the ceiling
    if (cod_rng_f64(rng) < 0.8) {
      CwGeoId pipe;
      if (!cwa_cache_find(A, CWK_CONDUIT, 0, 0, &pipe)) {
        CwGeo g = cw_cylinder(&A->al, 0.016f, 0.016f, 1.0f, 6, 1, false);
        cw_geo_fill_masks(&g, 0.35f, 0.5f, 0.1f);
        pipe = cwa_cache_store(A, CWK_CONDUIT, 0, 0, cwa_ready(A, g));
      }
      f32 runY = y + h - cod_rng_range(rng, 0.18f, 0.4f);
      f32 t0 = opening ? cw_wall_t(rng, true, half) : cod_rng_range(rng, -half, 0.0f);
      f32 t1;
      if (opening) {
        f32 sg = cw_sign(-t0 != 0.0f ? -t0 : 1.0f);
        t1 = t0 + sg * cod_rng_range(rng, 0.3f, 0.55f);
      } else {
        t1 = t0 + cod_rng_range(rng, 0.8f, m_maxf(1.0f, half - t0));
      }
      v2 rp = cw_wall_at(s, (t0 + t1) / 2, 0.045f);
      cwa_add(A, CW_METAL_DARK, pipe, LLR(IDENT, rp.x, runY, rp.y, s->yaw, 1, m_fabsf(t1 - t0), 1, 0, CW_PI_F / 2), NULL);
      f32 dropT = cod_rng_f64(rng) < 0.5 ? t0 : t1;
      f32 boxY = y + cod_rng_range(rng, 1.15f, 1.55f);
      v2 dp = cw_wall_at(s, dropT, 0.045f);
      cwa_add(A, CW_METAL_DARK, pipe, LL(IDENT, dp.x, (runY + boxY) / 2, dp.y, 0, 1, runY - boxY, 1), NULL);
      v2 bp = cw_wall_at(s, dropT, 0.055f);
      cwa_add(A, CW_METAL_DARK, BOX_FINE(A), LL(IDENT, bp.x, boxY, bp.y, s->yaw, 0.15f, 0.19f, 0.09f), CW_M(0.55f, 0.5f, 0.2f));
      if (cod_rng_f64(rng) < 0.5) {
        v2 fp = cw_wall_at(s, dropT + 0.06f, 0.05f);
        cwa_add(A, CW_METAL_DARK, pipe, LL(IDENT, fp.x, boxY - 0.28f, fp.y, 0, 0.4f, 0.34f, 0.4f), NULL);
      }
    }

    // a plank shelf on two brackets, with goods
    if (r->kind != CWR_RUIN && cod_rng_f64(rng) < 0.55) {
      f32 sy = y + cod_rng_range(rng, 1.05f, 1.65f);
      f32 sLen = m_minf(cod_rng_range(rng, opening ? 0.6f : 0.9f, opening ? 1.0f : 1.8f), s->len - 0.6f);
      f32 st = opening ? cw_wall_t(rng, true, half) : cod_rng_range(rng, -half + sLen / 2, half - sLen / 2);
      v2 sp = cw_wall_at(s, st, 0.15f);
      cwa_add(A, CW_WOOD_PROP_DARK, BOX(A), LL(IDENT, sp.x, sy, sp.y, s->yaw, sLen, 0.035f, 0.28f), CW_M(0.85f, 0.5f, 0.15f));
      for (i32 bt = -1; bt <= 1; bt += 2) {
        v2 bp = cw_wall_at(s, st + (f32)bt * (sLen / 2 - 0.12f), 0.1f);
        cwa_add(A, CW_METAL_DARK, BOX_FINE(A), LL(IDENT, bp.x, sy - 0.09f, bp.y, s->yaw, 0.03f, 0.16f, 0.18f), CW_M(0.6f, 0.6f, 0.3f));
      }
      for (i32 i = 0; i < cod_rng_int(rng, 2, 5); i++) {
        f32 gt = st + cod_rng_range(rng, -sLen / 2 + 0.12f, sLen / 2 - 0.12f);
        f32 goff = cod_rng_range(rng, 0.11f, 0.2f);
        v2 gp = cw_wall_at(s, gt, goff);
        u32 id = CW_PICK(rng, CW_L_SHELF_GOODS);
        f32 ry = cod_rng_float(rng) * CW_TAU32;
        f32 sc = cod_rng_range(rng, 0.6f, 0.95f);
        cwa_put_g(A, id, gp.x, sy + 0.02f, gp.y, ry, sc, 1.1f);
      }
    }

    // something leaning on it
    if (!opening && cod_rng_f64(rng) < 0.5) {
      f32 lean = cod_rng_range(rng, 0.13f, 0.22f);
      f32 lt = cod_rng_range(rng, -half, half);
      f32 lh = cod_rng_range(rng, 1.1f, 1.8f);
      f32 lw = cod_rng_range(rng, 0.5f, 1.0f);
      f32 off = 0.06f + (m_sinf(lean) * lh) / 2;
      v2 lp = cw_wall_at(s, lt, off);
      u32 key = CW_PICK(rng, CW_L_LEAN);
      f32 leanSign = s->nz != 0.0f ? -s->nz : -s->nx;
      cwa_add(A, key, BOX_THIN(A), LLR(IDENT, lp.x, y + (m_cosf(lean) * lh) / 2, lp.y, s->yaw, lw, lh, 0.022f, leanSign * lean, 0), CW_M(0.7f, 0.55f, 0.3f));
    }

    // objects standing against the skirting
    i32 nBase = cod_rng_int(rng, 2, 5);
    for (i32 i = 0; i < nBase; i++) {
      f32 bt = cod_rng_range(rng, -half, half);
      f32 boff = cod_rng_range(rng, 0.18f, 0.42f);
      v2 bp = cw_wall_at(s, bt, boff);
      u32 id = CW_PICK(rng, CW_L_BASE);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.8f, 1.05f);
      cwa_put_g(A, id, bp.x, y + 0.01f, bp.y, ry, sc, 1.2f);
    }

    // swept dust and plaster fall in the junction
    i32 nWedge = (i32)m_maxf(2.0f, m_roundf(s->len / 1.5f));
    for (i32 i = 0; i < nWedge; i++) {
      f32 wt = (((f32)i + cod_rng_range(rng, 0.2f, 0.8f)) / (f32)nWedge - 0.5f) * s->len;
      f32 woff = cod_rng_range(rng, 0.05f, 0.3f);
      v2 wp = cw_wall_at(s, wt, woff);
      f32 prad = cod_rng_range(rng, 0.3f, 0.75f);
      CwGeoId g = cwa_patch(A, rng, prad, 9, 0.55f);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      f32 sz = cod_rng_range(rng, 0.35f, 0.6f);
      cwa_add(A, CW_DIRT, g, LL(IDENT, wp.x, y + 0.011f, wp.y, ry, 1, 1, sz), CW_M(0.1f, 0.85f, 0.55f));
      if (cod_rng_f64(rng) < 0.7) {
        f32 ct = wt + cod_rng_range(rng, -0.3f, 0.3f);
        f32 coff = cod_rng_range(rng, 0.06f, 0.34f);
        v2 cp = cw_wall_at(s, ct, coff);
        u32 id = CW_PICK(rng, CW_L_WEDGE);
        f32 cry = cod_rng_float(rng) * CW_TAU32;
        f32 csc = cod_rng_range(rng, 0.45f, 0.9f);
        cwa_put_g(A, id, cp.x, y + 0.03f, cp.y, cry, csc, 1.4f);
      }
    }

    // a sack or a cloth hung on a nail
    if (cod_rng_f64(rng) < 0.45) {
      f32 ht = cw_wall_t(rng, opening, half);
      v2 hp = cw_wall_at(s, ht, 0.05f);
      f32 hy = y + cod_rng_range(rng, 1.3f, 1.85f);
      f32 cw_ = cod_rng_range(rng, 0.45f, 0.8f);
      f32 ch = cod_rng_range(rng, 0.6f, 1.0f);
      CwClothSpec cs = cw_cloth_defaults(cw_, ch);
      cs.seg_x = 6;
      cs.seg_y = 7;
      cs.sag = 0.1f;
      cs.wrinkle = 0.09f;
      cs.thickness = 0.003f;
      cs.fray = 0.014f;
      cs.seed = cw_cloth_seed(rng);
      CwGeoId cl = cwa_cloth(A, cs);
      u32 key = CW_PICK(rng, CW_L_FABRIC3);
      cwa_add(A, key, cl, LL(IDENT, hp.x, hy, hp.y, s->yaw, 1, 1, 1), CW_M(0.35f, 0.6f, 0.3f));
      cwa_add(A, CW_METAL_DARK, BOX_FINE(A), LL(IDENT, hp.x, hy + 0.34f, hp.y, s->yaw, 0.02f, 0.02f, 0.05f), CW_M(0.7f, 0.5f, 0.0f));
    }
  }
}

// exposed structure overhead: joists the bulb can rim-light
hz_internal void cw_dress_ceiling(CwA *A, CodRng *rng, const CwRoom *r) {
  f32 x0 = r->x0, z0 = r->z0, x1 = r->x1, z1 = r->z1, y = r->y, h = r->h;
  f32 w = x1 - x0, d = z1 - z0;
  if (h < 2.1f || w < 1.6f || d < 1.6f)
    return;
  b32 alongX = w < d;
  f32 span = alongX ? w : d;
  f32 runLen = alongX ? d : w;
  i32 n = (i32)m_maxf(2.0f, m_roundf(runLen / cod_rng_range(rng, 0.75f, 1.15f)));
  for (i32 i = 1; i < n; i++) {
    f32 t = ((f32)i / (f32)n - 0.5f) * runLen;
    f32 jx = alongX ? (x0 + x1) / 2 : (x0 + x1) / 2 + t;
    f32 jz = alongX ? (z0 + z1) / 2 + t : (z0 + z1) / 2;
    f32 sz = cod_rng_range(rng, 0.055f, 0.075f);
    cwa_add(A, CW_WOOD_PROP_DARK, BOX(A), LL(IDENT, jx, y + h - 0.06f, jz, alongX ? 0.0f : CW_PI_F / 2, span - 0.05f, 0.11f, sz), CW_M(0.35f, 0.6f, 0.45f));
  }
}

// ------------------------------------------------------------------ rooms --

hz_internal void cw_furnish_shop(CwA *A, CodRng *rng, const CwRoom *r, f32 cx, f32 cz, f32 w, f32 d) {
  f32 x0 = r->x0, z0 = r->z0, x1 = r->x1, z1 = r->z1, y = r->y;
  f32 frontZ = r->street == 0 ? -1.0f : r->street == 2 ? 1.0f : 0.0f;
  {
    f32 rx = cx + cod_rng_range(rng, -0.5f, 0.5f);
    f32 rz = cz + cod_rng_range(rng, -0.5f, 0.5f);
    f32 rs = cod_rng_range(rng, 1.6f, 2.4f);
    cw_add_rug(A, rng, rx, y, rz, rs);
  }
  b32 alongZ = r->street == 1 || r->street == 3;
  f32 ccx = alongZ ? (r->street == 1 ? x1 - 1.3f : x0 + 1.3f) : cx;
  f32 ccz = alongZ ? cz : (frontZ != 0.0f ? cz - frontZ * (d * 0.5f - 1.3f) : cz + d * 0.18f);
  f32 clen = m_minf((alongZ ? d : w) - 1.4f, 4.4f);
  f32 cSX = alongZ ? 0.74f : clen;
  f32 cSZ = alongZ ? clen : 0.74f;
  cwa_add(A, CW_WOOD_PROP_DARK, BOX(A), LL(IDENT, ccx, y + 0.9f, ccz, 0, cSX, 0.06f, cSZ), CW_M(0.9f, 0.4f, 0.1f));
  cwa_add(A, CW_WOOD_PROP_DARK, BOX(A),
          LL(IDENT, ccx + (alongZ ? -0.32f : 0.0f), y + 0.45f, ccz + (alongZ ? 0.0f : 0.32f), 0, alongZ ? 0.09f : cSX, 0.9f, alongZ ? cSZ : 0.09f),
          CW_M(0.5f, 0.6f, 0.4f));
  cwa_add(A, CW_WOOD_PROP_DARK, BOX(A), LL(IDENT, ccx, y + 0.28f, ccz, 0, cSX - 0.2f, 0.04f, cSZ - 0.2f), CW_M(0.4f, 0.7f, 0.5f));
  cwa_box(A, COD_SURFACE_WOOD, ccx, y + 0.45f, ccz, cSX, 0.9f, cSZ, 0.0f);
  for (u32 i = 0; i < 6; i++) {
    f32 t = cod_rng_range(rng, -clen / 2 + 0.3f, clen / 2 - 0.3f);
    f32 off = alongZ ? cod_rng_range(rng, -0.22f, 0.22f) : 0.0f;
    f32 px = alongZ ? ccx + off : ccx + t;
    f32 off2 = alongZ ? 0.0f : cod_rng_range(rng, -0.22f, 0.22f);
    f32 pz = alongZ ? ccz + t : ccz + off2;
    if (cod_rng_f64(rng) < 0.45) {
      f32 ry = cod_rng_range(rng, -0.4f, 0.4f) + (alongZ ? CW_PI_F / 2 : 0.0f);
      cwa_put_g(A, CWP_TRAY, px, y + 0.94f, pz, ry, 1.0f, 1.1f);
      f32 pry = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, CWP_PRODUCE, px, y + 0.96f, pz, pry, 1.0f, 1.0f);
    } else {
      u32 id = CW_PICK(rng, CW_L_SHOP_COUNTER);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.6f, 0.9f);
      cwa_put_g(A, id, px, y + 0.94f, pz, ry, sc, 1.15f);
    }
  }
  for (i32 i = 0; i < cod_rng_int(rng, 3, 6); i++) {
    f32 t = cod_rng_range(rng, -clen / 2, clen / 2);
    f32 off = cod_rng_range(rng, 0.55f, 1.05f);
    f32 px = ccx + (alongZ ? (r->street == 1 ? off : -off) : t);
    f32 pz = ccz + (alongZ ? t : (frontZ != 0.0f ? frontZ * off : off));
    u32 id = CW_PICK(rng, CW_L_SHOP_FRONT);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sc = cod_rng_range(rng, 0.9f, 1.05f);
    f32 g = cod_rng_range(rng, 1.0f, 1.3f);
    cwa_put_g(A, id, px, y + 0.02f + (f32)(i % 2) * 0.16f, pz, ry, sc, g);
  }
  for (i32 sx = -1; sx <= 1; sx += 2) {
    if ((r->street == 1 && sx > 0) || (r->street == 3 && sx < 0))
      continue;
    i32 n = (i32)m_maxf(1.0f, m_floorf(d / 1.5f) - 1.0f);
    for (i32 i = 0; i < n; i++) {
      f32 sz = z0 + 0.8f + (f32)i * 1.35f;
      if (sz > z1 - 0.7f)
        break;
      if (cod_rng_f64(rng) < 0.25)
        continue;
      f32 ry = sx > 0 ? -CW_PI_F / 2 : CW_PI_F / 2;
      f32 sc = cod_rng_range(rng, 0.92f, 1.08f);
      f32 g = cod_rng_range(rng, 0.8f, 1.4f);
      cwa_put_g(A, CWP_SHELF, cx + (f32)sx * (w / 2 - 0.22f), y, sz, ry, sc, g);
      for (u32 k = 0; k < 3; k++) {
        u32 id = CW_PICK(rng, CW_L_SHOP_SHELF);
        f32 jx = cod_rng_range(rng, -0.1f, 0.1f);
        f32 jz = cod_rng_range(rng, -0.35f, 0.35f);
        f32 gry = cod_rng_float(rng) * CW_TAU32;
        f32 gsc = cod_rng_range(rng, 0.7f, 1.1f);
        cwa_put_g(A, id, cx + (f32)sx * (w / 2 - 0.24f) + jx, y + 0.25f + (f32)k * 0.55f, sz + jz, gry, gsc, 1.2f);
      }
    }
  }
  cw_stack_crates(A, rng, x0 + 0.7f, y, z1 - 0.9f, cod_rng_int(rng, 3, 6));
  for (i32 i = 0; i < cod_rng_int(rng, 3, 6); i++) {
    u32 id = CW_PICK(rng, CW_L_SANDBAG3);
    f32 px = cod_rng_range(rng, x0 + 0.4f, x0 + 1.6f);
    f32 pz = cod_rng_range(rng, z0 + 0.5f, z0 + 2.0f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sc = cod_rng_range(rng, 0.9f, 1.1f);
    cwa_put_g(A, id, px, y + 0.02f + (f32)(i % 2) * 0.19f, pz, ry, sc, 1.2f);
  }
  {
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    cwa_put_g(A, CWP_BARREL_WOOD, x1 - 0.6f, y, z0 + 0.7f, ry, 1.0f, 1.2f);
    f32 ry2 = cod_rng_range(rng, -0.4f, 0.4f);
    cwa_put_g(A, CWP_TABLE_SMALL, cx - w * 0.28f, y, cz - d * 0.28f, ry2, 1.0f, 1.0f);
    f32 ry3 = cod_rng_range(rng, 2.0f, 4.0f);
    cwa_put_g(A, CWP_CHAIR, cx - w * 0.28f + 0.7f, y, cz - d * 0.2f, ry3, 1.0f, 1.2f);
    cwa_box(A, COD_SURFACE_WOOD, cx - w * 0.28f, y + 0.4f, cz - d * 0.28f, 1.0f, 0.8f, 0.8f, 0.0f);
  }
}

hz_internal void cw_furnish_living(CwA *A, CodRng *rng, const CwRoom *r, f32 cx, f32 cz) {
  f32 x0 = r->x0, z0 = r->z0, x1 = r->x1, z1 = r->z1, y = r->y;
  {
    f32 rs = cod_rng_range(rng, 2.0f, 2.8f);
    cw_add_rug(A, rng, cx, y, cz, rs);
  }
  {
    f32 ry = cod_rng_range(rng, -0.1f, 0.1f);
    cwa_put_g(A, CWP_MATTRESS, x0 + 1.1f, y, z1 - 0.9f, ry, 1.0f, 1.1f);
  }
  cwa_box(A, COD_SURFACE_FABRIC, x0 + 1.1f, y + 0.1f, z1 - 0.9f, 1.9f, 0.2f, 0.9f, 0.0f);
  {
    CwClothSpec cs = cw_cloth_defaults(1.5f, 0.9f);
    cs.seg_x = 7;
    cs.seg_y = 6;
    cs.sag = 0.05f;
    cs.wrinkle = 0.05f;
    cs.thickness = 0.0032f;
    cs.fray = 0.012f;
    cs.seed = cw_cloth_seed(rng);
    cwa_add(A, CW_FABRIC_TEAL, cwa_cloth(A, cs), LLR(IDENT, x0 + 1.2f, y + 0.19f, z1 - 1.0f, 0, 1, 1, 1, -CW_PI_F / 2, 0), CW_M(0.3f, 0.5f, 0.2f));
  }
  for (u32 i = 0; i < 3; i++) {
    CwGeo g = cw_chamfer_box(&A->al, 0.42f, 0.14f, 0.42f, 0.06f);
    cw_geo_fill_masks(&g, 0.2f, 0.4f, 0.2f);
    CwGeoId id = cwa_ready(A, g);
    u32 key = CW_PICK(rng, CW_L_FABRIC3);
    f32 px = cx + cod_rng_range(rng, -1.0f, 1.0f);
    f32 pz = cz + cod_rng_range(rng, -1.0f, 1.0f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    cwa_add(A, key, id, LL(IDENT, px, y + 0.07f, pz, ry, 1, 1, 1), NULL);
  }
  {
    f32 z = cz + cod_rng_range(rng, -0.6f, 0.6f);
    cwa_put_g(A, CWP_CABINET, x1 - 0.35f, y, z, -CW_PI_F / 2, 1.0f, 1.0f);
    cwa_box(A, COD_SURFACE_WOOD, x1 - 0.35f, y + 0.6f, cz, 0.5f, 1.2f, 0.9f, 0.0f);
    f32 ry = cod_rng_range(rng, 0.0f, 0.4f);
    cwa_put_g(A, CWP_TABLE_SMALL, cx + 0.4f, y, cz - 0.8f, ry, 1.0f, 1.0f);
    f32 ry2 = cod_rng_range(rng, 1.5f, 2.5f);
    cwa_put_g(A, CWP_CHAIR, cx - 0.8f, y, cz - 1.2f, ry2, 1.0f, 1.2f);
    f32 ry3 = cod_rng_range(rng, -1.5f, -0.5f);
    cwa_put_g(A, CWP_CHAIR, cx + 1.4f, y, cz - 0.4f, ry3, 1.0f, 1.2f);
    cwa_put_g(A, CWP_BOTTLE, cx + 0.4f, y + 0.74f, cz - 0.8f, 0.0f, 1.0f, 1.0f);
    cwa_put_g(A, CWP_CAN, cx + 0.6f, y + 0.74f, cz - 0.7f, 1.0f, 1.0f, 1.0f);
  }
  {
    CwClothSpec cs = cw_cloth_defaults(1.7f, 1.1f);
    cs.seg_x = 8;
    cs.seg_y = 7;
    cs.sag = 0.04f;
    cs.wrinkle = 0.05f;
    cs.thickness = 0.0036f;
    cs.fray = 0.02f;
    cs.bow = -1.0f;
    cs.seed = cw_cloth_seed(rng);
    cwa_add(A, CW_FABRIC_RED, cwa_cloth(A, cs), LL(IDENT, cx - 0.4f, y + 1.65f, z0 + 0.09f, 0, 1, 1, 1), CW_M(0.3f, 0.4f, 0.2f));
  }
  cw_stack_crates(A, rng, x1 - 0.9f, y, z0 + 0.8f, cod_rng_int(rng, 1, 3));
}

hz_internal void cw_furnish_storage(CwA *A, CodRng *rng, const CwRoom *r) {
  f32 x0 = r->x0, z0 = r->z0, x1 = r->x1, z1 = r->z1, y = r->y;
  i32 spots = cod_rng_int(rng, 4, 7);
  for (i32 i = 0; i < spots; i++) {
    f32 sx = cod_rng_range(rng, x0 + 0.6f, x1 - 0.6f);
    f32 sz = cod_rng_range(rng, z0 + 0.6f, z1 - 0.6f);
    f32 pick = cod_rng_float(rng);
    if (pick < 0.35f) {
      cw_stack_crates(A, rng, sx, y, sz, cod_rng_int(rng, 2, 5));
    } else if (pick < 0.55f) {
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, CWP_PALLET, sx, y + 0.01f, sz, ry, 1.0f, 1.3f);
      for (i32 k = 0; k < cod_rng_int(rng, 1, 4); k++) {
        u32 id = CW_PICK(rng, CW_L_STORE_PALLET);
        f32 jx = cod_rng_range(rng, -0.3f, 0.3f);
        f32 jz = cod_rng_range(rng, -0.25f, 0.25f);
        f32 gry = cod_rng_float(rng) * CW_TAU32;
        cwa_put_g(A, id, sx + jx, y + 0.11f + (f32)k * 0.2f, sz + jz, gry, 1.0f, 1.2f);
      }
      cwa_box(A, COD_SURFACE_WOOD, sx, y + 0.1f, sz, 1.2f, 0.2f, 1.0f, 0.0f);
    } else if (pick < 0.72f) {
      u32 id = CW_PICK(rng, CW_L_STORE_BARREL);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, id, sx, y, sz, ry, 1.0f, 1.2f);
      cwa_box(A, COD_SURFACE_METAL, sx, y + 0.45f, sz, 0.62f, 0.9f, 0.62f, 0.0f);
    } else if (pick < 0.85f) {
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, CWP_TYRE, sx, y, sz, ry, 1.0f, 1.3f);
      if (cod_rng_f64(rng) < 0.6) {
        A->skirts = false;
        f32 ry2 = cod_rng_float(rng) * CW_TAU32;
        cwa_put_g(A, CWP_TYRE, sx + 0.03f, y + 0.19f, sz + 0.02f, ry2, 1.0f, 1.3f);
        A->skirts = true;
      }
    } else {
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, CWP_SHELF, sx, y, sz, ry, 1.0f, 1.2f);
    }
  }
  for (i32 i = 0; i < cod_rng_int(rng, 2, 5); i++) {
    f32 px = cod_rng_range(rng, x0 + 0.5f, x1 - 0.5f);
    f32 pz = cod_rng_range(rng, z0 + 0.5f, z1 - 0.5f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    cwa_put_g(A, CWP_PLANK_A, px, y + 0.02f, pz, ry, 1.0f, 1.3f);
  }
  f32 ry = cod_rng_float(rng) * CW_TAU32;
  cwa_put_g(A, CWP_JERRY_CAN, x1 - 0.5f, y, z1 - 0.5f, ry, 1.0f, 1.3f);
  f32 ry2 = cod_rng_float(rng) * CW_TAU32;
  cwa_put_g(A, CWP_BUCKET, x0 + 0.5f, y, z1 - 0.6f, ry2, 1.0f, 1.4f);
}

hz_internal void cw_rubble_mound(CwA *A, CodRng *rng, f32 x, f32 y, f32 z, f32 radius, u32 count, u32 key);

hz_internal void cw_furnish_ruin(CwA *A, CodRng *rng, const CwRoom *r, f32 cx, f32 cz) {
  f32 x0 = r->x0, z0 = r->z0, x1 = r->x1, z1 = r->z1, y = r->y;
  {
    f32 mx = cx + cod_rng_range(rng, -1.0f, 1.0f);
    f32 mz = cz + cod_rng_range(rng, -1.0f, 1.0f);
    f32 mr = cod_rng_range(rng, 1.4f, 2.2f);
    cw_rubble_mound(A, rng, mx, y, mz, mr, 22, CW_CONCRETE);
  }
  for (i32 i = 0; i < cod_rng_int(rng, 3, 6); i++) {
    f32 px = cod_rng_range(rng, x0 + 0.5f, x1 - 0.5f);
    f32 pz = cod_rng_range(rng, z0 + 0.5f, z1 - 0.5f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    cwa_put_g(A, CWP_SLAB_SHARD, px, y + 0.05f, pz, ry, 1.0f, 1.4f);
  }
  for (i32 i = 0; i < cod_rng_int(rng, 6, 12); i++) {
    u32 id = CW_PICK(rng, CW_L_RUIN_RUBBLE);
    f32 px = cod_rng_range(rng, x0 + 0.3f, x1 - 0.3f);
    f32 pz = cod_rng_range(rng, z0 + 0.3f, z1 - 0.3f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sc = cod_rng_range(rng, 0.6f, 1.3f);
    cwa_put_g(A, id, px, y + 0.06f, pz, ry, sc, 1.5f);
  }
  {
    f32 px = cx + cod_rng_range(rng, -1.0f, 1.0f);
    f32 pz = cz + cod_rng_range(rng, -1.0f, 1.0f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    cwa_put_g(A, CWP_REBAR, px, y + 0.06f, pz, ry, 1.0f, 1.4f);
  }
  for (u32 i = 0; i < 3; i++) {
    f32 px = cod_rng_range(rng, x0 + 0.4f, x1 - 0.4f);
    f32 pz = cod_rng_range(rng, z0 + 0.4f, z1 - 0.4f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    cwa_put_g(A, CWP_PLANK_B, px, y + 0.03f, pz, ry, 1.0f, 1.4f);
  }
  {
    f32 px = cod_rng_range(rng, x0 + 0.6f, x1 - 0.6f);
    f32 pz = cod_rng_range(rng, z0 + 0.6f, z1 - 0.6f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    cwa_put_g(A, CWP_CHAIR, px, y + 0.05f, pz, ry, 1.0f, 1.5f);
  }
  CwClothSpec cs = cw_cloth_defaults(1.4f, 1.1f);
  cs.seg_x = 7;
  cs.seg_y = 7;
  cs.sag = 0.24f;
  cs.wrinkle = 0.075f;
  cs.twist = 0.08f;
  cs.fray = 0.02f;
  cs.seed = cw_cloth_seed(rng);
  CwGeoId sheet = cwa_cloth(A, cs);
  f32 sx = cx + cod_rng_range(rng, -1.5f, 1.5f);
  f32 sz = cz + cod_rng_range(rng, -1.5f, 1.5f);
  f32 sry = cod_rng_float(rng) * CW_TAU32;
  cwa_add(A, CW_FABRIC_CREAM, sheet, LLR(IDENT, sx, y + 0.55f, sz, sry, 1, 1, 1, -1.2f, 0), CW_M(0.4f, 0.7f, 0.3f));
}

// furnish one room; rect is level space, y the floor surface
hz_internal void cw_furnish_room(CwA *A, CodRng *rng, const CwRoom *r) {
  f32 w = m_fabsf(r->x1 - r->x0), d = m_fabsf(r->z1 - r->z0);
  if (w < 1.2f || d < 1.2f)
    return;
  f32 cx = (r->x0 + r->x1) / 2, cz = (r->z0 + r->z1) / 2;
  f32 x0 = r->x0, z0 = r->z0, x1 = r->x1, z1 = r->z1, y = r->y;

  i32 patches = cod_rng_int(rng, 2, 4);
  for (i32 i = 0; i < patches; i++) {
    f32 prad = cod_rng_range(rng, 0.4f, 1.1f);
    CwGeoId g = cwa_patch(A, rng, prad, 8, 0.5f);
    f32 px = cod_rng_range(rng, x0 + 0.3f, x1 - 0.3f);
    f32 pz = cod_rng_range(rng, z0 + 0.3f, z1 - 0.3f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    cwa_add(A, CW_DIRT, g, LL(IDENT, px, y + 0.012f, pz, ry, 1, 1, 1), CW_M(0.1f, 0.8f, 0.5f));
  }
  for (i32 i = 0; i < cod_rng_int(rng, 4, 9); i++) {
    f32 px = cod_rng_range(rng, x0 + 0.2f, x1 - 0.2f);
    f32 pz = cod_rng_range(rng, z0 + 0.2f, z1 - 0.2f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sc = cod_rng_range(rng, 0.7f, 1.3f);
    cwa_put_g(A, CWP_LITTER, px, y + 0.015f, pz, ry, sc, 1.3f);
  }
  for (i32 i = 0; i < cod_rng_int(rng, 2, 5); i++) {
    u32 id = CW_PICK(rng, CW_L_RUBBLE3);
    f32 px = cod_rng_range(rng, x0 + 0.25f, x1 - 0.25f);
    f32 pz = cod_rng_range(rng, z0 + 0.25f, z1 - 0.25f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sc = cod_rng_range(rng, 0.5f, 1.0f);
    cwa_put_g(A, id, px, y + 0.04f, pz, ry, sc, 1.4f);
  }

  switch (r->kind) {
  case CWR_SHOP: cw_furnish_shop(A, rng, r, cx, cz, w, d); break;
  case CWR_LIVING: cw_furnish_living(A, rng, r, cx, cz); break;
  case CWR_STORAGE: cw_furnish_storage(A, rng, r); break;
  case CWR_RUIN: cw_furnish_ruin(A, rng, r, cx, cz); break;
  default: cw_furnish_storage(A, rng, r); break;
  }

  cw_dress_walls(A, rng, r);
  cw_dress_ceiling(A, rng, r);

  if (r->kind != CWR_RUIN || cod_rng_f64(rng) < 0.5) {
    f32 bx = cx + cod_rng_range(rng, -0.8f, 0.8f);
    f32 bz = cz + cod_rng_range(rng, -0.8f, 0.8f);
    cw_hanging_bulb(A, rng, bx, y + r->h - 0.05f, bz);
  }
}
