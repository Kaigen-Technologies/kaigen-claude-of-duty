// Claude of Duty world street dressing: stalls, barriers, sandbags, wrecks, palms, lamps, cables, debris.

hz_internal const u16 CW_L_PEBBLE[] = {CWP_ROCK_B, CWP_ROCK_B, CWP_BRICK_B, CWP_CINDER, CWP_ROCK_A, CWP_LITTER};
hz_internal const u16 CW_L_DRIFT_JUNK[] = {CWP_BRICK_A, CWP_BRICK_B, CWP_CINDER, CWP_ROCK_A, CWP_ROCK_B, CWP_SLAB_SHARD, CWP_LITTER, CWP_CAN};
hz_internal const u16 CW_L_SPILL[] = {CWP_SLAB_SHARD, CWP_BRICK_A, CWP_BRICK_B, CWP_CINDER, CWP_ROCK_A, CWP_REBAR, CWP_PLANK_B};
hz_internal const u16 CW_L_CAR_JUNK[] = {CWP_GLASS_SHARDS, CWP_BRICK_B, CWP_ROCK_B, CWP_LITTER, CWP_CAN, CWP_SLAB_SHARD};
hz_internal const u16 CW_L_DRUM_MIX0[] = {CWP_BARREL_RUST, CWP_BARREL_RUST, CWP_BARREL_BLUE};
hz_internal const u16 CW_L_DRUM_MIX1[] = {CWP_BARREL_BLUE, CWP_BARREL_RUST, CWP_BARREL_WOOD};
hz_internal const u16 CW_L_DRUM_MIX2[] = {CWP_BARREL_RUST, CWP_BARREL_WOOD, CWP_BARREL_RUST};
hz_internal const u16 CW_L_TARP[] = {CW_FABRIC_TEAL, CW_FABRIC_CREAM, CW_BURLAP};
hz_internal const u16 CW_L_CANOPY_B[] = {CW_FABRIC_CREAM, CW_FABRIC_TEAL, CW_FABRIC_RED};
hz_internal const u16 CW_L_STALL_GOODS[] = {CWP_BOX_CARD_A, CWP_BOX_CARD_B, CWP_CRATE_B, CWP_BUCKET};
hz_internal const u16 CW_L_STALL_UNDER[] = {CWP_CRATE_A, CWP_CRATE_B, CWP_CRATE_FLAT, CWP_SANDBAG_A, CWP_TRAY};
hz_internal const u16 CW_L_SANDBAG2[] = {CWP_SANDBAG_A, CWP_SANDBAG_B};
hz_internal const u16 CW_L_BARRIER_OBJ[] = {CWP_TYRE, CWP_CRATE_A, CWP_BARREL_RUST, CWP_BLOCK_SMALL};
hz_internal const u16 CW_L_BARRIER_JUNK[] = {CWP_BRICK_A, CWP_BRICK_B, CWP_ROCK_B, CWP_LITTER};
hz_internal const u16 CW_L_BAG_BEHIND[] = {CWP_JERRY_CAN, CWP_CRATE_B, CWP_BOX_CARD_A, CWP_GAS_BOTTLE};
hz_internal const u16 CW_L_WRECK_JUNK[] = {CWP_BRICK_B, CWP_ROCK_B, CWP_LITTER, CWP_PLANK_B, CWP_CAN, CWP_GLASS_SHARDS};
hz_internal const u16 CW_L_LAMP_JUNK[] = {CWP_LITTER, CWP_BRICK_B, CWP_CAN, CWP_WEEDS};
hz_internal const u16 CW_L_FABRIC4[] = {CW_FABRIC_RED, CW_FABRIC_TEAL, CW_FABRIC_CREAM, CW_BURLAP};
hz_internal const u16 CW_L_FABRIC_RC[] = {CW_FABRIC_RED, CW_FABRIC_CREAM};
hz_internal const u16 CW_L_COVER_OBJ[] = {CWP_CRATE_C, CWP_BARREL_RUST, CWP_BLOCK_SMALL};
hz_internal const u16 CW_L_COVER_JUNK[] = {CWP_BRICK_A, CWP_BRICK_B, CWP_LITTER, CWP_CAN, CWP_ROCK_B, CWP_PLANK_A};

#define CW_PI2 1.5707963f

// open spots kept free of dressing, level space
hz_internal b32 cw_cam_clear(f32 x, f32 z, f32 r) {
  static const f32 pts[7][2] = {{0.0f, 20.0f}, {1.1f, 25.6f}, {-3.3f, 10.6f}, {-0.55f, 10.0f}, {-1.25f, 4.8f}, {-0.11f, 4.3f}, {-8.86f, 6.8f}};
  for (u32 i = 0; i < 7; i++) {
    f32 dx = x - pts[i][0], dz = z - pts[i][1];
    if (dx * dx + dz * dz < (r + 1.5f) * (r + 1.5f))
      return false;
  }
  return true;
}

// a dirt halo, a darker contact ring and kicked-out pebbles under a heavy prop; pebbles < 0 draws the count here
hz_internal void cw_ground_skirt(CwA *A, CodRng *rng, f32 x, f32 y, f32 z, f32 radius, u32 key, f32 grime, f32 ao, i32 pebbles) {
  f32 rr0 = radius * cod_rng_range(rng, 1.15f, 1.55f);
  CwGeoId g = cwa_patch(A, rng, rr0, 11, 0.5f);
  f32 yo = cod_rng_range(rng, 0.0f, 0.005f);
  f32 ry = cod_rng_float(rng) * CW_TAU32;
  f32 sz = cod_rng_range(rng, 0.7f, 1.0f);
  cwa_add(A, key ? key : CW_DIRT, g, LL(IDENT, x, y + 0.011f + yo, z, ry, 1, 1, sz), CW_M(0.08f, grime > 0.0f ? grime : 0.85f, ao > 0.0f ? ao : 0.55f));
  f32 r2 = radius * cod_rng_range(rng, 0.75f, 1.0f);
  CwGeoId g2 = cwa_patch(A, rng, r2, 9, 0.35f);
  f32 yo2 = cod_rng_range(rng, 0.0f, 0.004f);
  f32 ry2 = cod_rng_float(rng) * CW_TAU32;
  cwa_add(A, CW_DIRT, g2, LL(IDENT, x, y + 0.018f + yo2, z, ry2, 1, 1, 0.85f), CW_M(0.05f, 1.0f, 0.8f));
  i32 n = pebbles >= 0 ? pebbles : cod_rng_int(rng, 4, 8);
  for (i32 i = 0; i < n; i++) {
    f32 a = cod_rng_float(rng) * CW_TAU32;
    f32 rr = radius * cod_rng_range(rng, 0.75f, 1.5f);
    f32 px = x + m_cosf(a) * rr, pz = z + m_sinf(a) * rr;
    if (!cw_is_open(px, pz, 0.05f))
      continue;
    u32 id = CW_PICK(rng, CW_L_PEBBLE);
    f32 pry = cod_rng_float(rng) * CW_TAU32;
    f32 sc = cod_rng_range(rng, 0.45f, 0.95f);
    f32 g_ = cod_rng_range(rng, 1.1f, 1.5f);
    f32 rx = cod_rng_range(rng, -0.3f, 0.3f);
    f32 rz = cod_rng_range(rng, -0.3f, 0.3f);
    cwa_put_gr(A, id, px, cw_ground_y(px, pz) + 0.012f, pz, pry, sc, g_, rx, rz);
  }
}

// ------------------------------------------------------------- sandbags --

hz_internal void cw_sandbag_wall(CwA *A, CodRng *rng, f32 x, f32 z, f32 ry, f32 len, i32 courses, b32 has_base, f32 base_y) {
  f32 y = has_base ? base_y : cw_ground_y(x, z);
  f32 BAG_W = 0.5f, BAG_H = 0.17f;
  static const u16 IDS[3] = {CWP_SANDBAG_A, CWP_SANDBAG_B, CWP_SANDBAG_C};
  f32 cy = y + 0.01f;
  i32 prev = -1;
  for (i32 c = 0; c < courses; c++) {
    f32 load = (f32)(courses - 1 - c) / m_maxf(1.0f, (f32)(courses - 1));
    f32 squash = 1.0f - load * 0.19f;
    f32 spread = 1.0f + load * 0.07f;
    f32 pitch = BAG_W - cod_rng_range(rng, 0.02f, 0.04f);
    i32 per = (i32)m_maxf(2.0f, m_roundf(len / pitch));
    f32 stagger = (f32)(c % 2) * pitch * 0.5f + cod_rng_range(rng, -0.03f, 0.03f);
    i32 shrink = (c == courses - 1 && courses > 2) ? 1 : 0;
    f32 bagH = BAG_H * squash;
    for (i32 i = shrink; i < per - shrink; i++) {
      f32 lx = -len / 2 + stagger + ((f32)i + 0.5f) * pitch;
      if (m_fabsf(lx) > len / 2)
        continue;
      i32 pick = cod_rng_int(rng, 0, 2);
      if (pick == prev)
        pick = (pick + 1 + cod_rng_int(rng, 0, 1)) % 3;
      prev = pick;
      b32 header = cod_rng_f64(rng) < 0.3;
      f32 lz = cod_rng_range(rng, -0.03f, 0.03f);
      if (header)
        lz += cod_rng_range(rng, -0.05f, 0.05f);
      f32 px = x + m_cosf(ry) * lx + m_sinf(ry) * lz;
      f32 pz = z - m_sinf(ry) * lx + m_cosf(ry) * lz;
      f32 bry = ry + (header ? CW_PI_F / 2 : 0.0f) + cod_rng_range(rng, -0.21f, 0.21f);
      f32 sx = cod_rng_range(rng, 0.9f, 1.12f) * spread;
      f32 sy = cod_rng_range(rng, 0.9f, 1.06f) * squash;
      f32 sz = cod_rng_range(rng, 0.94f, 1.12f) * spread;
      f32 m1 = cod_rng_range(rng, 0.7f, 1.6f);
      f32 m2 = cod_rng_range(rng, 0.85f, 1.3f);
      f32 rx = cod_rng_range(rng, -0.09f, 0.09f);
      f32 rz = cod_rng_range(rng, -0.11f, 0.11f);
      cwa_put_s_m(A, IDS[pick], px, cy, pz, bry, sx, sy, sz, 1.0f, m1, m2, rx, rz);
    }
    cy += bagH - cod_rng_range(rng, 0.015f, 0.025f);
  }
  f32 h = m_maxf(0.2f, cy - y + 0.06f);
  cwa_box(A, COD_SURFACE_FABRIC, x, y + h / 2, z, len, h, 0.46f, ry);
  if (has_base)
    return;
  i32 skirts = (i32)m_maxf(2.0f, m_roundf(len / 1.1f));
  for (i32 i = 0; i < skirts; i++) {
    f32 lx = -len / 2 + (((f32)i + 0.5f) / (f32)skirts) * len;
    i32 peb = cod_rng_int(rng, 1, 3);
    cw_ground_skirt(A, rng, x + m_cosf(ry) * lx, y, z - m_sinf(ry) * lx, 0.44f, CW_SAND, 0.7f, 0.0f, peb);
  }
  for (i32 i = 0; i < cod_rng_int(rng, 1, 3); i++) {
    f32 lx = cod_rng_range(rng, -len / 2, len / 2);
    f32 px = x + m_cosf(ry) * lx + m_sinf(ry) * 0.7f;
    f32 pz = z - m_sinf(ry) * lx + m_cosf(ry) * 0.7f;
    if (!cw_is_open(px, pz, 0.3f))
      continue;
    u32 id = CW_PICK(rng, CW_L_BAG_BEHIND);
    f32 pry = cod_rng_float(rng) * CW_TAU32;
    cwa_put_g(A, id, px, cw_ground_y(px, pz), pz, pry, 1.0f, 1.3f);
  }
}

// each tyre dropped on the last: the stack walks sideways, leans, and every tyre is turned off its neighbour
hz_internal void cw_tyre_stack(CwA *A, CodRng *rng, f32 x, f32 y, f32 z, i32 n) {
  f32 walkA = cod_rng_float(rng) * CW_TAU32;
  f32 lean = cod_rng_range(rng, -0.05f, 0.05f);
  f32 ox = 0, oz = 0;
  f32 yaw = cod_rng_float(rng) * CW_TAU32;
  for (i32 i = 0; i < n; i++) {
    f32 a = walkA + cod_rng_range(rng, -1.1f, 1.1f);
    f32 step = cod_rng_range(rng, 0.02f, 0.04f);
    ox += m_cosf(a) * step;
    oz += m_sinf(a) * step;
    f32 sg = cod_rng_f64(rng) < 0.5 ? -1.0f : 1.0f;
    yaw += sg * cod_rng_range(rng, 0.087f, 0.262f);
    f32 sx = cod_rng_range(rng, 0.97f, 1.04f);
    f32 sy = cod_rng_range(rng, 0.9f, 1.05f);
    f32 sz = cod_rng_range(rng, 0.97f, 1.04f);
    f32 m1 = cod_rng_range(rng, 0.88f, 1.35f);
    f32 m2 = cod_rng_range(rng, 0.9f, 1.2f);
    f32 rx = lean * cod_rng_range(rng, 0.5f, 1.5f);
    f32 rz = cod_rng_range(rng, -0.05f, 0.05f);
    cwa_put_s_m(A, i % 2 ? CWP_TYRE_SMALL : CWP_TYRE, x + ox, y + (f32)i * 0.168f, z + oz, yaw, sx, sy, sz, 1.0f, m1, m2, rx, rz);
  }
}

// ---------------------------------------------------------- street floor --

hz_internal void cw_street_floor(CwA *A, CodRng *rng) {
  const f32 HW = CW_STREET_HW, KB = CW_STREET_KERB, WH = CW_STREET_WALK_H, zMin = CW_STREET_ZMIN, zMax = CW_STREET_ZMAX;

  // 0. the wall-to-ground junction: a stained band on each plinth face and a fillet of grit
  for (i32 side = -1; side <= 1; side += 2) {
    f32 z = zMin;
    while (z < zMax) {
      f32 seg = cod_rng_range(rng, 0.5f, 1.1f);
      f32 cz = z + seg / 2;
      b32 host = false;
      for (u32 bi = 0; bi < CW_BUILDING_COUNT; bi++) {
        const CwBuildingSpec *b = &CW_BUILDINGS[bi];
        if (m_fabsf(m_fabsf(b->x) - b->w / 2 - KB) > 0.3f)
          continue;
        if (cw_sign(b->x) != (f32)side)
          continue;
        if (cz > b->z - b->d / 2 + 0.05f && cz < b->z + b->d / 2 - 0.05f) {
          host = true;
          break;
        }
      }
      if (host) {
        f32 h = cod_rng_range(rng, 0.15f, 0.25f);
        f32 px = (f32)side * (KB + 0.056f);
        cwa_add(A, CW_CONCRETE, BOX_THIN(A), LL(IDENT, px, WH + h / 2 - 0.025f, cz, 0, 0.034f, h, seg * 0.99f), CW_M(0.0f, 1.0f, 0.85f));
        if (cod_rng_f64(rng) < 0.75) {
          f32 bw = cod_rng_range(rng, 0.16f, 0.34f);
          f32 bh = cod_rng_range(rng, 0.04f, 0.09f);
          CwGeoId g = cwa_drift(A, rng, seg * 0.95f, bw, bh, 3);
          cwa_add(A, CW_DIRT, g, LL(IDENT, (f32)side * (KB - 0.04f), WH - 0.012f, cz, side > 0 ? CW_PI2 : -CW_PI2, 1, 1, 1), CW_M(0.1f, 0.95f, 0.7f));
        }
      }
      z += seg;
    }
  }

  // 1. drift berms banked against the building line
  for (i32 side = -1; side <= 1; side += 2) {
    f32 z = zMin + 1;
    while (z < zMax - 2) {
      f32 len = cod_rng_range(rng, 2.2f, 6.5f);
      f32 cz = z + len / 2;
      f32 x = (f32)side * (KB - 0.06f);
      if (cw_is_open(x - (f32)side * 0.5f, cz, 0.05f) && cod_rng_f64(rng) < 0.96) {
        f32 h = cod_rng_range(rng, 0.14f, 0.42f);
        f32 w = cod_rng_range(rng, 0.6f, 1.5f);
        CwGeoId g = cwa_drift(A, rng, len, w, h, 4);
        u32 key = cod_rng_f64(rng) < 0.72 ? CW_SAND : CW_ROAD_DUST;
        cwa_add(A, key, g, LL(IDENT, x, WH - 0.02f, cz, side > 0 ? CW_PI2 : -CW_PI2, 1, 1, 1), CW_M(0.15f, 0.55f, 0.45f));
        for (i32 i = 0; i < cod_rng_int(rng, 2, 6); i++) {
          f32 px = x - (f32)side * cod_rng_range(rng, 0.05f, w * 0.8f);
          f32 pz = cz + cod_rng_range(rng, -len / 2 + 0.2f, len / 2 - 0.2f);
          u32 id = CW_PICK(rng, CW_L_DRIFT_JUNK);
          f32 py = WH + h * cod_rng_range(rng, 0.1f, 0.55f);
          f32 ry = cod_rng_float(rng) * CW_TAU32;
          f32 sc = cod_rng_range(rng, 0.6f, 1.15f);
          f32 gg = cod_rng_range(rng, 1.1f, 1.5f);
          f32 rx = cod_rng_range(rng, -0.25f, 0.25f);
          f32 rz = cod_rng_range(rng, -0.25f, 0.25f);
          cwa_put_gr(A, id, px, py, pz, ry, sc, gg, rx, rz);
        }
      }
      z += len + cod_rng_range(rng, 0.1f, 0.9f);
    }
  }

  // 2. the kerb line: sand spilling off the pavement into the gutter
  for (i32 i = 0; i < 70; i++) {
    f32 side = cod_rng_f64(rng) < 0.5 ? -1.0f : 1.0f;
    f32 cz = cod_rng_range(rng, zMin + 2, zMax - 2);
    f32 len = cod_rng_range(rng, 1.2f, 3.4f);
    if (!cw_is_open(side * (HW + 0.4f), cz, 0.05f))
      continue;
    f32 bw = cod_rng_range(rng, 0.35f, 0.8f);
    f32 bh = cod_rng_range(rng, 0.05f, 0.14f);
    CwGeoId g = cwa_drift(A, rng, len, bw, bh, 3);
    cwa_add(A, CW_SAND, g, LL(IDENT, side * (HW + 0.12f), 0.02f, cz, side > 0 ? -CW_PI2 : CW_PI2, 1, 1, 1), CW_M(0.15f, 0.5f, 0.3f));
  }

  // 3. tyre tracks polished into the dust along the driving line
  for (i32 side = -1; side <= 1; side += 2) {
    f32 z = zMin + 2;
    while (z < zMax - 3) {
      f32 len = cod_rng_range(rng, 5.0f, 13.0f);
      f32 x = (f32)side * cod_rng_range(rng, 1.25f, 1.95f);
      f32 camber = (1.0f - (x / HW) * (x / HW)) * 0.055f + 0.038f;
      CwGeoId g = cwa_patch(A, rng, 0.34f, 13, 0.28f);
      f32 ry = cod_rng_range(rng, -0.03f, 0.03f);
      cwa_add(A, CW_ROAD_RUT, g, LL(IDENT, x, camber, z + len / 2, ry, 1, 1, len / 0.68f), CW_M(0.55f, 0.5f, 0.15f));
      if (cod_rng_f64(rng) < 0.7) {
        CwGeoId hg = cwa_patch(A, rng, 0.62f, 11, 0.4f);
        f32 hry = cod_rng_range(rng, -0.04f, 0.04f);
        cwa_add(A, CW_ROAD_DUST, hg, LL(IDENT, x, camber - 0.004f, z + len / 2, hry, 1, 1, len / 1.24f), CW_M(0.45f, 0.15f, 0.08f));
      }
      if (cod_rng_f64(rng) < 0.6) {
        CwGeoId dg = cwa_drift(A, rng, len * 0.8f, 0.3f, 0.035f, 3);
        cwa_add(A, CW_ROAD_DUST, dg, LL(IDENT, x - (f32)side * 0.42f, camber + 0.004f, z + len / 2, CW_PI2, 1, 1, 1), CW_M(0.1f, 0.4f, 0.2f));
      }
      z += len + cod_rng_range(rng, 0.5f, 4.0f);
    }
  }
  for (i32 i = 0; i < 8; i++) {
    f32 z = cod_rng_range(rng, zMin + 5, zMax - 5);
    f32 prad = cod_rng_range(rng, 0.5f, 1.1f);
    CwGeoId g = cwa_patch(A, rng, prad, 12, 0.5f);
    f32 x = cod_rng_range(rng, -HW + 0.6f, HW - 0.6f);
    f32 ry = cod_rng_float(rng) * 3.14f;
    f32 sz = cod_rng_range(rng, 1.4f, 2.6f);
    cwa_add(A, CW_ASPHALT, g, LL(IDENT, x, (1.0f - (x / HW) * (x / HW)) * 0.055f + 0.04f, z, ry, 1, 1, sz), CW_M(0.45f, 0.4f, 0.15f));
  }

  // 4. masonry spill: chunks that fell off the buildings onto the kerb
  for (i32 i = 0; i < 120; i++) {
    f32 side = cod_rng_f64(rng) < 0.5 ? -1.0f : 1.0f;
    f32 z = cod_rng_range(rng, zMin + 1, zMax - 1);
    f32 x = side * (KB - m_fabsf(cod_rng_gauss(rng)) * 1.5f - 0.1f);
    if (!cw_is_open(x, z, 0.05f))
      continue;
    f32 y = cw_ground_y(x, z);
    u32 id = CW_PICK(rng, CW_L_SPILL);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sc = cod_rng_range(rng, 0.7f, 1.35f);
    f32 gg = cod_rng_range(rng, 1.0f, 1.5f);
    f32 rx = cod_rng_range(rng, -0.3f, 0.3f);
    f32 rz = cod_rng_range(rng, -0.3f, 0.3f);
    cwa_put_gr(A, id, x, y + 0.02f, z, ry, sc, gg, rx, rz);
  }
  static const f32 spills[8][2] = {{-5.4f, 16.5f}, {5.5f, 11.0f}, {-5.6f, 2.0f}, {5.6f, -4.0f}, {-5.5f, -13.5f}, {5.4f, -19.0f}, {-5.3f, -25.5f}, {5.5f, -33.0f}};
  for (u32 i = 0; i < 8; i++) {
    f32 x = spills[i][0], z = spills[i][1];
    if (!cw_is_open(x, z, 0.1f) || !cw_cam_clear(x, z, 1.8f))
      continue;
    f32 rad = cod_rng_range(rng, 1.1f, 1.9f);
    i32 cnt = cod_rng_int(rng, 18, 30);
    cw_rubble_mound(A, rng, x, cw_ground_y(x, z), z, rad, (u32)cnt, CW_CONCRETE_PROP);
  }

  // 5. silhouette breakers at eye level in the mid-ground
  {
    f32 cx = -3.35f, cz = -6.2f, cry = 0.28f;
    if (cw_cam_clear(cx, cz, 2.6f)) {
      f32 y = cw_ground_y(cx, cz);
      cwa_put_g(A, CWP_WRECK, cx, y + 0.02f, cz, cry, 1.0f, 0.85f);
      cwa_box(A, COD_SURFACE_METAL, cx, y + 0.75f, cz, 1.85f, 1.5f, 4.4f, cry);
      CwGeoId dg = cwa_drift(A, rng, 4.2f, 0.7f, 0.13f, 3);
      cwa_add(A, CW_SAND, dg, LL(IDENT, cx - 1.0f, y + 0.005f, cz, cry + CW_PI2, 1, 1, 1), CW_M(0.15f, 0.6f, 0.5f));
      A->skirts = false;
      cwa_put_gr(A, CWP_TYRE, cx + 1.5f, y + 0.1f, cz - 1.8f, 1.1f, 1.0f, 1.4f, 1.5f, 0.2f);
      A->skirts = true;
      for (i32 i = 0; i < 12; i++) {
        f32 px = cx + cod_rng_range(rng, -2.2f, 2.2f);
        f32 pz = cz + cod_rng_range(rng, -3.0f, 3.0f);
        if (!cw_is_open(px, pz, 0.1f))
          continue;
        u32 id = CW_PICK(rng, CW_L_CAR_JUNK);
        f32 ry = cod_rng_float(rng) * CW_TAU32;
        f32 sc = cod_rng_range(rng, 0.6f, 1.2f);
        cwa_put_g(A, id, px, cw_ground_y(px, pz) + 0.015f, pz, ry, sc, 1.4f);
      }
    }
  }

  // oil drum clusters: each with its own barrel mix, radius, damage and dressing
  {
    static const f32 clusters[3][3] = {{-5.1f, -2.0f, 5}, {4.9f, -11.5f, 4}, {4.75f, 6.2f, 3}};
    static const f32 spreads[3] = {0.62f, 0.8f, 0.5f};
    static const f32 lyings[3] = {0.28f, 0.1f, 0.45f};
    u32 cluster = 0;
    for (u32 ci = 0; ci < 3; ci++) {
      f32 dx = clusters[ci][0], dz = clusters[ci][1];
      i32 n = (i32)clusters[ci][2];
      const u16 *mix = cluster % 3 == 0 ? CW_L_DRUM_MIX0 : cluster % 3 == 1 ? CW_L_DRUM_MIX1 : CW_L_DRUM_MIX2;
      f32 spread = spreads[cluster % 3];
      f32 lyingP = lyings[cluster % 3];
      f32 phase = cod_rng_float(rng) * CW_TAU32;
      cluster++;
      if (!cw_cam_clear(dx, dz, 1.4f))
        continue;
      b32 have_tall = false;
      v3 tallest = V3_ZERO;
      for (i32 i = 0; i < n; i++) {
        f32 a = phase + ((f32)i / (f32)n) * CW_TAU32 + cod_rng_range(rng, -0.5f, 0.5f);
        f32 r = i == 0 ? 0.0f : cod_rng_range(rng, spread * 0.85f, spread * 1.4f);
        f32 px = dx + m_cosf(a) * r, pz = dz + m_sinf(a) * r;
        if (!cw_is_open(px, pz, 0.2f))
          continue;
        b32 lying = i > 0 && cod_rng_f64(rng) < (f64)lyingP;
        f32 y = cw_ground_y(px, pz);
        u32 id = mix[cod_rng_pick(rng, 3)];
        f32 ry = cod_rng_float(rng) * CW_TAU32;
        f32 gg = cod_rng_range(rng, 1.1f, 1.5f);
        f32 rz = lying ? 0.0f : cod_rng_range(rng, -0.03f, 0.03f);
        cwa_put_gr(A, id, px, y + (lying ? 0.3f : 0.0f), pz, ry, 1.0f, gg, lying ? CW_PI_F / 2 : 0.0f, rz);
        cwa_box(A, COD_SURFACE_METAL, px, y + (lying ? 0.3f : 0.45f), pz, 0.64f, lying ? 0.6f : 0.9f, 0.64f, 0.0f);
        if (!lying) {
          i32 peb = cod_rng_int(rng, 2, 5);
          cw_ground_skirt(A, rng, px, y, pz, 0.36f, 0, 0.0f, 0.0f, peb);
          if (!have_tall) {
            have_tall = true;
            tallest = (v3){px, y, pz};
          }
        }
      }
      f32 qx = dx + cod_rng_range(rng, -1.2f, 1.2f);
      f32 qz = dz + cod_rng_range(rng, -1.2f, 1.2f);
      f32 qry = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, CWP_PLANK_A, qx, cw_ground_y(dx, dz) + 0.03f, qz, qry, 1.2f, 1.4f);
      if (have_tall && cod_rng_f64(rng) < 0.55) {
        f32 cw_ = cod_rng_range(rng, 1.0f, 1.5f);
        f32 ch = cod_rng_range(rng, 0.9f, 1.3f);
        CwClothSpec cs = cw_cloth_defaults(cw_, ch);
        cs.seg_x = 8;
        cs.seg_y = 8;
        cs.sag = 0.22f;
        cs.wrinkle = 0.055f;
        cs.twist = 0.1f;
        cs.thickness = 0.003f;
        cs.fray = 0.02f;
        cs.seed = cw_cloth_seed(rng);
        CwGeoId cloth = cwa_cloth(A, cs);
        u32 key = CW_PICK(rng, CW_L_TARP);
        f32 ry = cod_rng_float(rng) * CW_TAU32;
        f32 gm = cod_rng_range(rng, 0.55f, 0.9f);
        cwa_add(A, key, cloth, LLR(IDENT, tallest.x, tallest.y + 0.86f, tallest.z, ry, 1, 1, 1, -1.35f, 0), CW_M(0.35f, gm, 0.25f));
      }
    }
  }

  // a tyre pile and a pallet stack, on the pavement so they never block the road
  {
    static const f32 piles[3][3] = {{-5.5f, 6.2f, 0}, {5.55f, -1.2f, 1}, {5.45f, -26.5f, 0}};
    for (u32 i = 0; i < 3; i++) {
      f32 px = piles[i][0], pz = piles[i][1];
      b32 tyres = piles[i][2] == 0;
      if (!cw_cam_clear(px, pz, 1.2f))
        continue;
      f32 y = cw_ground_y(px, pz);
      if (tyres) {
        i32 n = cod_rng_int(rng, 5, 8);
        cw_tyre_stack(A, rng, px, y, pz, n);
        cw_ground_skirt(A, rng, px, y, pz, 0.45f, 0, 0.0f, 0.0f, -1);
        cwa_box(A, COD_SURFACE_RUBBER, px, y + ((f32)n * 0.172f) / 2, pz, 0.7f, (f32)n * 0.172f, 0.7f, 0.0f);
      } else {
        i32 n = cod_rng_int(rng, 4, 7);
        for (i32 k = 0; k < n; k++) {
          f32 jx = cod_rng_range(rng, -0.07f, 0.07f);
          f32 jz = cod_rng_range(rng, -0.07f, 0.07f);
          f32 ry = cod_rng_range(rng, -0.12f, 0.12f);
          f32 gg = cod_rng_range(rng, 1.0f, 1.4f);
          cwa_put_g(A, CWP_PALLET, px + jx, y + (f32)k * 0.135f, pz + jz, ry, 1.0f, gg);
        }
        cwa_box(A, COD_SURFACE_WOOD, px, y + ((f32)n * 0.135f) / 2, pz, 1.2f, (f32)n * 0.135f, 0.9f, 0.0f);
        f32 cry = cod_rng_float(rng) * CW_TAU32;
        cwa_put_g(A, CWP_CRATE_B, px + 0.75f, y, pz + 0.5f, cry, 1.0f, 1.3f);
        i32 peb = cod_rng_int(rng, 3, 6);
        cw_ground_skirt(A, rng, px, y, pz, 0.72f, 0, 0.0f, 0.0f, peb);
      }
    }
  }
}

// ----------------------------------------------------------- market stalls --

hz_internal void cw_market_stalls(CwA *A, CodRng *rng) {
  for (u32 si = 0; si < 8; si++) {
    f32 x = CW_STALLS[si].x, z = CW_STALLS[si].z, w = CW_STALLS[si].w;
    f32 y = cw_ground_y(x, z);
    f32 s = w / 2.3f;
    f32 ry = CW_STALLS[si].ry + cod_rng_range(rng, -0.07f, 0.07f);
    f32 sy = cod_rng_range(rng, 0.94f, 1.05f);
    f32 sz = cod_rng_range(rng, 0.95f, 1.06f);
    f32 gm = cod_rng_range(rng, 0.8f, 1.35f);
    cwa_put_s_m(A, CWP_STALL, x, y, z, ry, s, sy, sz, 1.0f, gm, 1.0f, 0.0f, 0.0f);
    cwa_box(A, COD_SURFACE_WOOD, x, y + 0.45f, z, w, 0.9f, 1.05f, ry);
    for (i32 ti = 0; ti < 2; ti++) {
      f32 tt = ti == 0 ? -0.42f : 0.42f;
      i32 peb = cod_rng_int(rng, 2, 5);
      cw_ground_skirt(A, rng, x + m_cosf(ry) * w * tt, y, z - m_sinf(ry) * w * tt, 0.4f, 0, 0.0f, 0.0f, peb);
    }
    f32 cw_ = w * cod_rng_range(rng, 1.02f, 1.16f);
    f32 cd = cod_rng_range(rng, 1.32f, 1.6f);
    u16 keys[2];
    keys[0] = (u16)cw_pick_fabric3(rng);
    keys[1] = CW_PICK(rng, CW_L_CANOPY_B);
    f32 slack = cod_rng_range(rng, 0.8f, 1.5f);
    f32 trz = cod_rng_range(rng, -0.05f, 0.05f);
    m4 cm = LLR(IDENT, x, y + 2.02f, z, ry, 1, 1, 1, -CW_PI_F / 2, trz);
    i32 skip = cod_rng_f64(rng) < 0.3 ? cod_rng_int(rng, 0, 5) : -1;
    f32 mg = cod_rng_range(rng, 0.4f, 0.7f);
    u32 k32[2] = {keys[0], keys[1]};
    CwStripedOpts so = {.seg_y = 7, .sag = 0.19f * slack, .wrinkle = 0.028f * slack, .bulge = 0.05f * slack, .bulge_set = true, .thickness = 0.0028f,
                        .fray = 0.012f, .skip_band = skip + 1, .has_masks = true, .masks = {0.35f, mg, 0.15f}};
    cw_striped_cloth(A, k32, 2, cm, cw_, cd, so, rng);
    f32 vh = cod_rng_range(rng, 0.24f, 0.4f);
    f32 vm = cod_rng_range(rng, 0.45f, 0.75f);
    CwStripedOpts vo = {.seg_y = 3, .sag = 0.06f * slack, .wrinkle = 0.028f * slack, .bulge_set = true, .bulge = 0.0f, .thickness = 0.0026f,
                        .fray = 0.016f, .has_masks = true, .masks = {0.4f, vm, 0.2f}};
    cw_striped_cloth(A, k32, 2, LL(IDENT, x, y + 1.86f, z, ry, 1, 1, 1), cw_, vh, vo, rng);
    if (cod_rng_f64(rng) < 0.55) {
      f32 sd = cod_rng_f64(rng) < 0.5 ? -1.0f : 1.0f;
      u32 kk[1] = {k32[cod_rng_int(rng, 0, 1)]};
      f32 dh = cod_rng_range(rng, 0.9f, 1.3f);
      f32 dm = cod_rng_range(rng, 0.5f, 0.8f);
      CwStripedOpts d_ = {.seg_x = 7, .seg_y = 8, .sag = 0.09f, .wrinkle = 0.042f, .twist = 0.07f, .bulge_set = false, .thickness = 0.0026f,
                          .fray = 0.02f, .has_masks = true, .masks = {0.35f, dm, 0.25f}};
      cw_striped_cloth(A, kk, 1, LL(IDENT, x + m_cosf(ry) * (cw_ / 2) * sd, y + 1.42f, z - m_sinf(ry) * (cw_ / 2) * sd, ry + CW_PI_F / 2, 1, 1, 1), cd * 0.9f, dh, d_, rng);
    }

    i32 n = cod_rng_int(rng, 3, 6);
    for (i32 i = 0; i < n; i++) {
      f32 lx = cod_rng_range(rng, -w / 2 + 0.3f, w / 2 - 0.3f);
      f32 lz = cod_rng_range(rng, -0.35f, 0.35f);
      f32 px = x + m_cosf(ry) * lx + m_sinf(ry) * lz;
      f32 pz = z - m_sinf(ry) * lx + m_cosf(ry) * lz;
      if (cod_rng_f64(rng) < 0.5) {
        f32 tr = ry + cod_rng_range(rng, -0.3f, 0.3f);
        cwa_put_g(A, CWP_TRAY, px, y + 0.87f, pz, tr, 1.0f, 1.1f);
        if (cod_rng_f64(rng) < 0.8) {
          f32 pr = cod_rng_float(rng) * CW_TAU32;
          cwa_put_g(A, CWP_PRODUCE, px, y + 0.89f, pz, pr, 1.0f, 1.0f);
        }
      } else {
        u32 id = CW_PICK(rng, CW_L_STALL_GOODS);
        f32 gr = cod_rng_float(rng) * CW_TAU32;
        f32 sc = cod_rng_range(rng, 0.7f, 1.0f);
        cwa_put_g(A, id, px, y + 0.87f, pz, gr, sc, 1.2f);
      }
    }
    for (i32 i = 0; i < cod_rng_int(rng, 2, 5); i++) {
      f32 lx = cod_rng_range(rng, -w / 2, w / 2);
      f32 lz = cod_rng_range(rng, -0.3f, 0.3f);
      u32 id = CW_PICK(rng, CW_L_STALL_UNDER);
      f32 gr = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.85f, 1.05f);
      f32 gg = cod_rng_range(rng, 1.0f, 1.4f);
      cwa_put_g(A, id, x + m_cosf(ry) * lx + m_sinf(ry) * lz, y + 0.02f, z - m_sinf(ry) * lx + m_cosf(ry) * lz, gr, sc, gg);
    }
    f32 sideX = x + m_cosf(ry) * (w / 2 + 0.5f), sideZ = z - m_sinf(ry) * (w / 2 + 0.5f);
    if (cw_is_open(sideX, sideZ, 0.4f)) {
      f32 br = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, CWP_BARREL_WOOD, sideX, cw_ground_y(sideX, sideZ), sideZ, br, 1.0f, 1.2f);
      cwa_box(A, COD_SURFACE_WOOD, sideX, y + 0.4f, sideZ, 0.66f, 0.8f, 0.66f, 0.0f);
    }
    f32 str = cod_rng_float(rng) * CW_TAU32;
    cwa_put_g(A, CWP_STOOL, x - m_sinf(ry) * 0.95f, y, z - m_cosf(ry) * 0.95f, str, 1.0f, 1.3f);
  }
}

// -------------------------------------------------------------- barriers --

hz_internal void cw_barriers(CwA *A, CodRng *rng) {
  for (u32 i = 0; i < 9; i++) {
    f32 x = CW_JERSEYS[i].x, z = CW_JERSEYS[i].z;
    f32 y = cw_ground_y(x, z);
    f32 jr = CW_JERSEYS[i].ry + cod_rng_range(rng, -0.05f, 0.05f);
    f32 gg = cod_rng_range(rng, 0.8f, 1.3f);
    f32 rz = cod_rng_range(rng, -0.02f, 0.02f);
    cwa_put_gr(A, CWP_JERSEY, x, y, z, jr, 1.0f, gg, 0.0f, rz);
    cwa_box(A, COD_SURFACE_CONCRETE, x, y + 0.46f, z, 0.62f, 0.92f, 1.9f, jr);
    for (i32 ti = 0; ti < 2; ti++) {
      f32 t = ti == 0 ? -0.55f : 0.55f;
      i32 peb = cod_rng_int(rng, 2, 4);
      cw_ground_skirt(A, rng, x + m_sinf(jr) * t * 1.1f, y, z + m_cosf(jr) * t * 1.1f, 0.52f, 0, 0.0f, 0.0f, peb);
    }
    if (cod_rng_f64(rng) < 0.4) {
      u32 id = CW_PICK(rng, CW_L_SANDBAG2);
      f32 px = x + cod_rng_range(rng, -0.5f, 0.5f);
      f32 pz = z + cod_rng_range(rng, -0.6f, 0.6f);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, id, px, y + 0.92f, pz, ry, 1.0f, 1.2f);
    }
    if (cod_rng_f64(rng) < 0.45) {
      f32 ox = x + m_cosf(jr) * cod_rng_range(rng, 0.5f, 0.9f);
      f32 oz = z - m_sinf(jr) * cod_rng_range(rng, 0.5f, 0.9f);
      u32 id = CW_PICK(rng, CW_L_BARRIER_OBJ);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, id, ox, cw_ground_y(ox, oz), oz, ry, 1.0f, 1.3f);
    }
    for (i32 k = 0; k < cod_rng_int(rng, 1, 4); k++) {
      u32 id = CW_PICK(rng, CW_L_BARRIER_JUNK);
      f32 jx = cod_rng_range(rng, -1.2f, 1.2f);
      f32 jz = cod_rng_range(rng, -1.4f, 1.4f);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.6f, 1.1f);
      cwa_put_g(A, id, x + jx, y + 0.03f, z + jz, ry, sc, 1.4f);
    }
  }

  static const f32 blocks[7][3] = {{-4.0f, 22.0f, 0.1f}, {4.2f, 14.5f, -0.15f}, {-4.3f, -1.0f, 0.05f}, {4.3f, -12.0f, 0.2f},
                                   {-4.1f, -30.0f, -0.1f}, {4.0f, -37.5f, 0.12f}, {-2.0f, -41.0f, 1.5f}};
  for (u32 i = 0; i < 7; i++) {
    f32 x = blocks[i][0], z = blocks[i][1], ry = blocks[i][2];
    f32 y = cw_ground_y(x, z);
    f32 gg = cod_rng_range(rng, 0.9f, 1.3f);
    cwa_put_g(A, CWP_BLOCK_BIG, x, y, z, ry, 1.0f, gg);
    cwa_box(A, COD_SURFACE_CONCRETE, x, y + 0.48f, z, 1.3f, 0.96f, 0.9f, ry);
    for (i32 ti = 0; ti < 2; ti++) {
      f32 t = ti == 0 ? -0.4f : 0.4f;
      i32 peb = cod_rng_int(rng, 2, 5);
      cw_ground_skirt(A, rng, x + m_cosf(ry) * t, y, z - m_sinf(ry) * t, 0.62f, 0, 0.0f, 0.0f, peb);
    }
    if (cod_rng_f64(rng) < 0.6) {
      f32 jx = cod_rng_range(rng, -1.0f, 1.0f);
      f32 jz = cod_rng_range(rng, -1.0f, 1.0f);
      f32 br = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, CWP_BLOCK_SMALL, x + jx, y, z + jz, br, 1.0f, 1.2f);
    }
  }
}

hz_internal void cw_sandbag_emplacements(CwA *A, CodRng *rng) {
  for (u32 i = 0; i < 4; i++)
    cw_sandbag_wall(A, rng, CW_SANDBAG_WALLS[i].x, CW_SANDBAG_WALLS[i].z, CW_SANDBAG_WALLS[i].ry, CW_SANDBAG_WALLS[i].len, 5, false, 0.0f);
}

hz_internal void cw_wrecks(CwA *A, CodRng *rng) {
  for (u32 wi = 0; wi < 3; wi++) {
    f32 x = CW_WRECKS[wi].x, z = CW_WRECKS[wi].z, ry = CW_WRECKS[wi].ry, roll = CW_WRECKS[wi].len;
    f32 y = cw_ground_y(x, z);
    cwa_put_gr(A, CWP_WRECK, x, y + 0.02f, z, ry, 1.0f, 1.0f, 0.0f, (roll * CW_PI_F) / 180.0f);
    cwa_box(A, COD_SURFACE_METAL, x, y + 0.75f, z, 1.85f, 1.5f, 4.4f, ry);
    static const f32 wheel[4][2] = {{0.86f, 1.35f}, {-0.86f, 1.35f}, {0.86f, -1.35f}, {-0.86f, -1.35f}};
    for (u32 i = 0; i < 4; i++) {
      if (i == 3)
        continue;
      f32 lx = wheel[i][0], lz = wheel[i][1];
      f32 px = x + m_cosf(ry) * lx + m_sinf(ry) * lz;
      f32 pz = z - m_sinf(ry) * lx + m_cosf(ry) * lz;
      cwa_put_g(A, CWP_WHEEL_FLAT, px, y + 0.2f, pz, ry, 1.0f, 1.2f);
    }
    cwa_put_g(A, CWP_BLOCK_SMALL, x + m_cosf(ry) * -0.86f + m_sinf(ry) * -1.35f, y, z - m_sinf(ry) * -0.86f + m_cosf(ry) * -1.35f, ry, 1.0f, 1.4f);
    f32 srad = cod_rng_range(rng, 2.6f, 3.4f);
    CwGeoId scorch = cwa_patch(A, rng, srad, 11, 0.5f);
    f32 sry = cod_rng_float(rng) * CW_TAU32;
    cwa_add(A, CW_ASPHALT, scorch, LL(IDENT, x, y + 0.008f, z, sry, 1, 1, 0.7f), CW_M(0.05f, 1.0f, 0.9f));
    for (i32 i = 0; i < 18; i++) {
      f32 a = cod_rng_float(rng) * CW_TAU32;
      f32 r = cod_rng_range(rng, 1.2f, 4.5f);
      f32 px = x + m_cosf(a) * r, pz = z + m_sinf(a) * r;
      if (!cw_is_open(px, pz, 0.2f))
        continue;
      u32 id = CW_PICK(rng, CW_L_WRECK_JUNK);
      f32 jr = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.6f, 1.2f);
      cwa_put_g(A, id, px, cw_ground_y(px, pz) + 0.02f, pz, jr, sc, 1.4f);
    }
    f32 tr = cod_rng_float(rng) * CW_TAU32;
    cwa_put_g(A, CWP_TYRE, x + m_cosf(ry) * 1.6f, y, z - m_sinf(ry) * 1.6f, tr, 1.0f, 1.3f);
  }
}

hz_internal void cw_palms(CwA *A, CodRng *rng) {
  for (u32 pi = 0; pi < 7; pi++) {
    f32 x = CW_PALMS[pi].x, z = CW_PALMS[pi].z, s = CW_PALMS[pi].s;
    f32 y = cw_ground_y(x, z);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 gg = cod_rng_range(rng, 0.8f, 1.2f);
    cwa_put_g(A, CWP_PALM_TRUNK, x, y, z, ry, s, gg);
    f32 topY = y + 5.4f * s;
    i32 n = cod_rng_int(rng, 8, 11);
    for (i32 i = 0; i < n; i++) {
      f32 a = ry + ((f32)i / (f32)n) * CW_TAU32 + cod_rng_range(rng, -0.16f, 0.16f);
      f32 tilt = cod_rng_range(rng, -0.55f, 0.15f);
      f32 dy = cod_rng_range(rng, 0.05f, 0.3f);
      f32 sx = s * cod_rng_range(rng, 0.85f, 1.15f);
      f32 sy = s * cod_rng_range(rng, 0.85f, 1.15f);
      f32 sz = s * cod_rng_range(rng, 0.85f, 1.15f);
      f32 gm = cod_rng_range(rng, 0.7f, 1.3f);
      cwa_put_s_m(A, CWP_PALM_FROND, x, topY - dy, z, a, sx, sy, sz, 1.0f, gm, 1.0f, 0.0f, tilt);
    }
    for (i32 i = 0; i < 3; i++) {
      f32 a = ry + cod_rng_float(rng) * CW_TAU32;
      cwa_put_s_m(A, CWP_PALM_FROND, x, topY - 0.35f, z, a, s * 0.8f, s * 0.8f, s * 0.8f, 1.0f, 1.6f, 1.0f, 0.0f, -1.35f);
    }
    cwa_box(A, COD_SURFACE_WOOD, x, y + 2.7f * s, z, 0.42f * s, 5.4f * s, 0.42f * s, 0.0f);
    f32 prad = cod_rng_range(rng, 0.9f, 1.4f);
    CwGeoId g = cwa_patch(A, rng, prad, 10, 0.45f);
    f32 pry = cod_rng_float(rng) * CW_TAU32;
    cwa_add(A, CW_DIRT, g, LL(IDENT, x, y + 0.02f, z, pry, 1, 1, 1), CW_M(0.1f, 0.8f, 0.5f));
    for (i32 i = 0; i < cod_rng_int(rng, 3, 7); i++) {
      f32 a = cod_rng_float(rng) * CW_TAU32;
      f32 r = cod_rng_range(rng, 0.4f, 1.2f);
      f32 wry = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.7f, 1.3f);
      cwa_put_g(A, CWP_WEEDS, x + m_cosf(a) * r, y + 0.02f, z + m_sinf(a) * r, wry, sc, 1.2f);
    }
    if (cod_rng_f64(rng) < 0.5) {
      f32 px = x + cod_rng_range(rng, -1.4f, 1.4f);
      f32 pz = z + cod_rng_range(rng, -1.4f, 1.4f);
      f32 pr = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, CWP_PLANTER, px, y, pz, pr, 1.0f, 1.3f);
    }
  }
}

hz_internal void cw_street_lamps(CwA *A, CodRng *rng) {
  for (u32 li = 0; li < 5; li++) {
    f32 x = CW_LAMPS[li].x, z = CW_LAMPS[li].z;
    f32 y = cw_ground_y(x, z);
    f32 ry = x < 0 ? 0.0f : CW_PI_F;
    f32 gg = cod_rng_range(rng, 0.9f, 1.2f);
    cwa_put_g(A, CWP_LAMP_POST, x, y, z, ry, 1.0f, gg);
    f32 armX = x + m_cosf(ry) * 0.88f, armZ = z - m_sinf(ry) * 0.88f;
    cwa_put(A, CWP_LAMP_GLASS, armX, y + 5.33f, armZ, ry, 1.0f, NULL, 0.0f, -0.16f);
    cwa_box(A, COD_SURFACE_METAL, x, y + 2.7f, z, 0.3f, 5.4f, 0.3f, 0.0f);
    i32 peb = cod_rng_int(rng, 3, 6);
    cw_ground_skirt(A, rng, x, y, z, 0.34f, 0, 0.0f, 0.0f, peb);
    dyn_arr_append(A->lamp_anchors, ((v3){armX, y + 5.3f, armZ}));
    if (cod_rng_f64(rng) < 0.5)
      cwa_put_g(A, CWP_SIGN_HANG, x + m_cosf(ry) * 0.2f, y + 3.4f, z - m_sinf(ry) * 0.2f, ry + CW_PI_F / 2, 1.0f, 1.2f);
    for (i32 i = 0; i < cod_rng_int(rng, 2, 5); i++) {
      f32 a = cod_rng_float(rng) * CW_TAU32;
      f32 r = cod_rng_range(rng, 0.35f, 1.1f);
      f32 px = x + m_cosf(a) * r, pz = z + m_sinf(a) * r;
      u32 id = CW_PICK(rng, CW_L_LAMP_JUNK);
      f32 jr = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.7f, 1.2f);
      cwa_put_g(A, id, px, cw_ground_y(px, pz) + 0.02f, pz, jr, sc, 1.3f);
    }
  }
}

hz_internal void cw_insulator(CwA *A, f32 x, f32 y, f32 z) {
  cwa_add(A, CW_CONCRETE_DARK, BOX_FINE(A), LL(IDENT, x, y, z, 0, 0.1f, 0.16f, 0.1f), CW_M(0.6f, 0.5f, 0.2f));
}

hz_internal void cw_overhead_lines(CwA *A, CodRng *rng) {
  for (u32 i = 0; i < 6; i++) {
    const CwSpan *c = &CW_CABLES[i];
    CwGeoId t = cwa_catenary(A, (v3){c->x0, c->y0, c->z0}, (v3){c->x1, c->y1, c->z1}, c->sag, 0.022f, 14, 4, 0.05f);
    cwa_add(A, CW_METAL_DARK, t, IDENT, CW_M(0.4f, 0.7f, 0.2f));
    CwGeoId t2 = cwa_catenary(A, (v3){c->x0, c->y0 - 0.22f, c->z0 + 0.18f}, (v3){c->x1, c->y1 - 0.18f, c->z1 + 0.2f}, c->sag * 1.12f, 0.014f, 14, 4, 0.06f);
    cwa_add(A, CW_METAL_DARK, t2, IDENT, CW_M(0.4f, 0.7f, 0.2f));
    cw_insulator(A, c->x0, c->y0 + 0.06f, c->z0);
    cw_insulator(A, c->x1, c->y1 + 0.06f, c->z1);
  }

  const f32 SAG = 0.42f;
  for (u32 i = 0; i < 6; i++) {
    const CwSpan *l = &CW_LAUNDRY[i];
    CwGeoId line = cwa_catenary(A, (v3){l->x0, l->y0, l->z0}, (v3){l->x1, l->y1, l->z1}, SAG, 0.012f, 12, 4, 0.0f);
    cwa_add(A, CW_METAL_DARK, line, IDENT, CW_M(0.3f, 0.6f, 0.2f));
    f32 dx = l->x1 - l->x0, dz = l->z1 - l->z0;
    f32 len = m_sqrtf(dx * dx + dz * dz);
    f32 ry = m_atan2f(-dz, dx);
    i32 n = (i32)m_maxf(2.0f, m_roundf(len / 1.7f));
    f32 K = cw_cosh(1.5f) - 1.0f;
    for (i32 k = 0; k < n; k++) {
      f32 t = ((f32)k + 0.5f) / (f32)n;
      if (cod_rng_f64(rng) < 0.12)
        continue;
      f32 droop = (cw_cosh(1.5f) - cw_cosh((t - 0.5f) * 3.0f)) / K;
      f32 px = l->x0 + dx * t, pz = l->z0 + dz * t;
      f32 py = l->y0 + (l->y1 - l->y0) * t - SAG * droop - 0.03f;
      f32 w = cod_rng_range(rng, 0.72f, 1.15f);
      f32 h = cod_rng_range(rng, 0.85f, 1.45f);
      CwClothSpec cs = cw_cloth_defaults(w, h);
      cs.seg_x = 9;
      cs.seg_y = 10;
      cs.sag = cod_rng_range(rng, 0.18f, 0.3f);
      cs.wrinkle = cod_rng_range(rng, 0.05f, 0.085f);
      cs.twist = cod_rng_range(rng, 0.1f, 0.2f);
      cs.bulge = 0.06f;
      cs.thickness = cod_rng_range(rng, 0.0016f, 0.003f);
      cs.fray = cod_rng_range(rng, 0.01f, 0.03f);
      cs.seed = cw_cloth_seed(rng);
      CwGeoId cloth = cwa_cloth(A, cs);
      u32 key = CW_PICK(rng, CW_L_FABRIC4);
      f32 gm = cod_rng_range(rng, 0.4f, 0.8f);
      cwa_add(A, key, cloth, LL(IDENT, px, py - h / 2 + 0.02f, pz, ry, 1, 1, 1), CW_M(0.3f, gm, 0.2f));
    }
  }
}

hz_internal void cw_facade_hangings(CwA *A, CodRng *rng) {
  for (u32 i = 0; i < 5; i++) {
    const CwHanging *hg = &CW_HANGINGS[i];
    CwClothSpec cs = cw_cloth_defaults(hg->w, hg->h);
    cs.seg_x = 10;
    cs.seg_y = 10;
    cs.sag = cod_rng_range(rng, 0.09f, 0.15f);
    cs.wrinkle = cod_rng_range(rng, 0.04f, 0.07f);
    cs.bulge = cod_rng_range(rng, 0.05f, 0.11f);
    cs.twist = cod_rng_range(rng, 0.03f, 0.1f);
    cs.thickness = cod_rng_range(rng, 0.0026f, 0.004f);
    cs.fray = cod_rng_range(rng, 0.015f, 0.035f);
    cs.bow = -1.0f;
    cs.seed = cw_cloth_seed(rng);
    CwGeoId cloth = cwa_cloth(A, cs);
    u32 key = cw_pick_fabric3(rng);
    f32 gm = cod_rng_range(rng, 0.42f, 0.72f);
    cwa_add(A, key, cloth, LL(IDENT, hg->x, hg->y, hg->z, hg->ry, 1, 1, 1), CW_M(0.35f, gm, 0.2f));
    cwa_add(A, CW_METAL_RUST, BOX_FINE(A), LL(IDENT, hg->x, hg->y + hg->h / 2 + 0.06f, hg->z, hg->ry, hg->w + 0.2f, 0.035f, 0.035f), CW_M(0.9f, 0.5f, 0.1f));
    if (cod_rng_f64(rng) < 0.6) {
      CwClothSpec c2 = cw_cloth_defaults(hg->w * 0.55f, hg->h * 0.7f);
      c2.seg_x = 7;
      c2.seg_y = 8;
      c2.sag = 0.12f;
      c2.wrinkle = 0.06f;
      c2.seed = cw_cloth_seed(rng);
      c2.thickness = 0.0032f;
      c2.fray = 0.025f;
      c2.bow = -1.0f;
      CwGeoId g2 = cwa_cloth(A, c2);
      u32 k2 = CW_PICK(rng, CW_L_FABRIC_RC);
      cwa_add(A, k2, g2, LL(IDENT, hg->x - m_sinf(hg->ry) * (hg->w * 0.75f), hg->y - 0.25f, hg->z - m_cosf(hg->ry) * (hg->w * 0.75f), hg->ry, 1, 1, 1), CW_M(0.4f, 0.6f, 0.25f));
    }
  }
}

hz_internal void cw_rubble_piles(CwA *A, CodRng *rng) {
  for (u32 i = 0; i < 5; i++) {
    f32 x = CW_RUBBLE[i].x, z = CW_RUBBLE[i].z, radius = CW_RUBBLE[i].radius;
    u32 count = CW_RUBBLE[i].count;
    f32 y = cw_ground_y(x, z);
    cw_rubble_mound(A, rng, x, y, z, radius, count, CW_CONCRETE);
    CwGeoId g = cwa_patch(A, rng, radius * 1.5f, 12, 0.4f);
    f32 pry = cod_rng_float(rng) * CW_TAU32;
    cwa_add(A, CW_DIRT, g, LL(IDENT, x, y + 0.012f, z, pry, 1, 1, 1), CW_M(0.1f, 0.9f, 0.6f));
    for (i32 k = 0; k < cod_rng_int(rng, 2, 5); k++) {
      f32 px = x + cod_rng_range(rng, -radius, radius);
      f32 pz = z + cod_rng_range(rng, -radius, radius);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, CWP_SLAB_SHARD, px, y + 0.06f, pz, ry, 1.0f, 1.3f);
    }
    for (i32 k = 0; k < cod_rng_int(rng, 1, 3); k++) {
      f32 px = x + cod_rng_range(rng, -radius, radius);
      f32 pz = z + cod_rng_range(rng, -radius, radius);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, CWP_REBAR, px, y + 0.05f, pz, ry, 1.0f, 1.4f);
    }
    for (i32 k = 0; k < cod_rng_int(rng, 3, 7); k++) {
      f32 px = x + cod_rng_range(rng, -radius * 1.4f, radius * 1.4f);
      f32 pz = z + cod_rng_range(rng, -radius * 1.4f, radius * 1.4f);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      f32 rx = cod_rng_range(rng, -0.2f, 0.2f);
      f32 rz = cod_rng_range(rng, -0.2f, 0.2f);
      cwa_put_gr(A, CWP_CINDER, px, y + 0.02f, pz, ry, 1.0f, 1.3f, rx, rz);
    }
  }
}

hz_internal void cw_tyre_stacks(CwA *A, CodRng *rng) {
  for (u32 i = 0; i < 4; i++) {
    f32 x = CW_TYRES[i].x, z = CW_TYRES[i].z;
    i32 n = (i32)CW_TYRES[i].n;
    f32 y = cw_ground_y(x, z);
    cw_tyre_stack(A, rng, x, y, z, n);
    cw_ground_skirt(A, rng, x, y, z, 0.42f, 0, 0.0f, 0.0f, -1);
    cwa_box(A, COD_SURFACE_RUBBER, x, y + ((f32)n * 0.175f) / 2, z, 0.68f, (f32)n * 0.175f, 0.68f, 0.0f);
    if (cod_rng_f64(rng) < 0.6) {
      A->skirts = false;
      f32 px = x + cod_rng_range(rng, 0.7f, 1.1f);
      f32 pz = z + cod_rng_range(rng, -0.6f, 0.6f);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      cwa_put_gr(A, CWP_TYRE, px, y, pz, ry, 1.0f, 1.3f, 1.4f, 0.0f);
      A->skirts = true;
    }
  }
}

hz_internal void cw_cover_clusters(CwA *A, CodRng *rng) {
  static const f32 spots[6][3] = {{0.6f, 0.9f, 0.35f}, {-2.2f, 8.6f, 1.2f}, {2.6f, -6.4f, -0.4f}, {-3.0f, -21.5f, 0.6f}, {2.2f, -33.0f, 1.9f}, {-2.6f, 27.5f, 0.2f}};
  for (u32 i = 0; i < 6; i++) {
    f32 x = spots[i][0], z = spots[i][1], ry = spots[i][2];
    f32 y = cw_ground_y(x, z);
    f32 len = cod_rng_range(rng, 1.8f, 2.8f);
    cw_sandbag_wall(A, rng, x, z, ry, len, 6, false, 0.0f);
    f32 bx = x + m_cosf(ry + 1.57f) * 1.5f, bz = z - m_sinf(ry + 1.57f) * 1.5f;
    if (cw_is_open(bx, bz, 0.4f)) {
      u32 id = CW_PICK(rng, CW_L_COVER_OBJ);
      f32 br = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, id, bx, cw_ground_y(bx, bz), bz, br, 1.0f, 1.2f);
      cwa_box(A, COD_SURFACE_WOOD, bx, y + 0.4f, bz, 0.8f, 0.8f, 0.8f, 0.0f);
      i32 peb = cod_rng_int(rng, 3, 6);
      cw_ground_skirt(A, rng, bx, cw_ground_y(bx, bz), bz, 0.5f, 0, 0.0f, 0.0f, peb);
    }
    for (i32 k = 0; k < cod_rng_int(rng, 3, 6); k++) {
      f32 px = x + cod_rng_range(rng, -2.0f, 2.0f);
      f32 pz = z + cod_rng_range(rng, -2.0f, 2.0f);
      if (!cw_is_open(px, pz, 0.2f))
        continue;
      u32 id = CW_PICK(rng, CW_L_COVER_JUNK);
      f32 jr = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.6f, 1.2f);
      cwa_put_g(A, id, px, cw_ground_y(px, pz) + 0.02f, pz, jr, sc, 1.4f);
    }
  }
}

hz_internal void cw_dress_street(CwA *A, CodRng *rng) {
  cwa_jitter_on(A);
  cw_market_stalls(A, rng);
  cw_barriers(A, rng);
  cw_sandbag_emplacements(A, rng);
  cw_wrecks(A, rng);
  cw_palms(A, rng);
  cw_street_lamps(A, rng);
  cw_overhead_lines(A, rng);
  cw_facade_hangings(A, rng);
  cw_rubble_piles(A, rng);
  cw_tyre_stacks(A, rng);
  cw_cover_clusters(A, rng);
  cw_street_floor(A, rng);
  cwa_jitter_off(A);
}
