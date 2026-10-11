// Claude of Duty world dressing: facade services, roof clutter, alley lines, scatter, gate and perimeter.

hz_internal const u16 CW_L_BALCONY[] = {CWP_CRATE_B, CWP_BUCKET, CWP_PLANTER, CWP_BOX_CARD_B, CWP_STOOL, CWP_JERRY_CAN, CWP_TYRE_SMALL};
hz_internal const u16 CW_L_DOORSTEP[] = {CWP_BUCKET, CWP_CRATE_B, CWP_STOOL, CWP_SANDBAG_A, CWP_LITTER, CWP_JERRY_CAN, CWP_PLANTER};
hz_internal const u16 CW_L_ROOF_CRATE[] = {CWP_CRATE_A, CWP_CRATE_B, CWP_CRATE_FLAT};
hz_internal const u16 CW_L_ROOF_MISC[] = {CWP_STOOL, CWP_CHAIR, CWP_TYRE, CWP_BARREL_RUST, CWP_PALLET, CWP_GAS_BOTTLE};
hz_internal const u16 CW_L_ROOF_JUNK[] = {CWP_BRICK_A, CWP_BRICK_B, CWP_ROCK_B, CWP_LITTER, CWP_CINDER, CWP_CAN, CWP_PLANK_B};
hz_internal const u16 CW_L_BRICK2[] = {CWP_BRICK_A, CWP_BRICK_B};
hz_internal const u16 CW_L_ROCK2[] = {CWP_ROCK_A, CWP_ROCK_B};
hz_internal const u16 CW_L_CAN_BOTTLE[] = {CWP_CAN, CWP_BOTTLE};
hz_internal const u16 CW_L_PLANK2[] = {CWP_PLANK_A, CWP_PLANK_B};
hz_internal const u16 CW_L_CARD2[] = {CWP_BOX_CARD_A, CWP_BOX_CARD_B};
hz_internal const u16 CW_L_SCATTER_MISC[] = {CWP_TYRE_SMALL, CWP_BUCKET, CWP_CRATE_B, CWP_SLAB_SHARD};
hz_internal const u16 CW_L_ROAD_JUNK[] = {CWP_LITTER, CWP_CAN, CWP_ROCK_B, CWP_BRICK_B, CWP_LITTER, CWP_BOTTLE, CWP_WEEDS};
hz_internal const u16 CW_L_ALLEY_BRICK[] = {CWP_BRICK_A, CWP_BRICK_B, CWP_CINDER};
hz_internal const u16 CW_L_ALLEY_CRATE[] = {CWP_CRATE_A, CWP_CRATE_B, CWP_CRATE_FLAT, CWP_PALLET};
hz_internal const u16 CW_L_ALLEY_BARREL[] = {CWP_BARREL_RUST, CWP_BARREL_BLUE, CWP_BARREL_WOOD};
hz_internal const u16 CW_L_TYRE2[] = {CWP_TYRE, CWP_TYRE_SMALL};
hz_internal const u16 CW_L_ALLEY_MISC[] = {CWP_BOX_CARD_A, CWP_BOX_CARD_B, CWP_BUCKET, CWP_JERRY_CAN};
hz_internal const u16 CW_L_ALLEY_HEAVY[] = {CWP_SLAB_SHARD, CWP_REBAR, CWP_GAS_BOTTLE};
hz_internal const u16 CW_L_GATE_JUNK[] = {CWP_BRICK_A, CWP_BRICK_B, CWP_ROCK_B, CWP_LITTER, CWP_CINDER, CWP_CAN, CWP_WEEDS, CWP_PLANK_B};
hz_internal const u16 CW_L_PERIM_WALL[] = {CW_PLASTER_SAND, CW_PLASTER_CREAM, CW_CONCRETE};
hz_internal const u16 CW_L_PERIM_JUNK[] = {CWP_BRICK_A, CWP_BRICK_B, CWP_CINDER, CWP_ROCK_A, CWP_SLAB_SHARD, CWP_REBAR};

// facade services and roof clutter, from the anchors the building returned
hz_internal void cw_dress_building(CwA *A, CodRng *rng, const CwBuildInfo *info) {
  const CwBuildingSpec *spec = info->spec;
  f32 top = info->roof_y;

  for (u32 wi = 0; wi < info->windows.len; wi++) {
    const CwWindowInfo *wnd = &info->windows.items[wi];
    m4 pm = wnd->pm;
    if (wnd->f == 0)
      continue;
    if (cod_rng_f64(rng) < 0.34) {
      f32 dx = (cod_rng_f64(rng) < 0.5 ? -1.0f : 1.0f) * (wnd->w / 2 + 0.55f);
      v3 wp = cw_world_of(pm, wnd->x + dx, wnd->y - 0.35f, -0.36f);
      f32 gg = cod_rng_range(rng, 0.8f, 1.3f);
      cwa_put_g(A, CWP_AC_UNIT, wp.x, wp.y, wp.z, cw_ry_of(pm) + CW_PI_F, 1.0f, gg);
      f32 runH = wnd->y - 1.1f;
      if (runH > 0.5f)
        cwa_add(A, CW_PLASTER_SAND, BOX_FINE(A), LL(pm, wnd->x + dx, wnd->y - 0.75f - runH / 2, -0.004f, 0, 0.16f, runH, 0.008f), CW_M(0.0f, 1.0f, 0.75f));
    }
    if (cod_rng_f64(rng) < 0.16) {
      f32 sg = cod_rng_f64(rng) < 0.5 ? -1.0f : 1.0f;
      v3 wp = cw_world_of(pm, wnd->x + (wnd->w / 2 + 0.4f) * sg, wnd->y + 0.3f, -0.07f);
      cwa_put_g(A, CWP_CONDUIT_BOX, wp.x, wp.y, wp.z, cw_ry_of(pm) + CW_PI_F, 1.0f, 1.2f);
    }
    if (cod_rng_f64(rng) < 0.18) {
      v3 a = cw_world_of(pm, wnd->x - wnd->w / 2 - 0.1f, wnd->y + 0.5f, -0.12f);
      v3 b = cw_world_of(pm, wnd->x + wnd->w / 2 + 0.1f, wnd->y + 0.45f, -0.12f);
      CwGeoId line = cwa_catenary(A, a, b, 0.08f, 0.008f, 6, 4, 0.0f);
      cwa_add(A, CW_METAL_DARK, line, IDENT, CW_M(0.3f, 0.6f, 0.0f));
      for (i32 i = 0; i < 2; i++) {
        f32 t = 0.3f + (f32)i * 0.4f;
        f32 cw_ = cod_rng_range(rng, 0.3f, 0.5f);
        f32 ch = cod_rng_range(rng, 0.4f, 0.7f);
        CwClothSpec cs = cw_cloth_defaults(cw_, ch);
        cs.seg_x = 5;
        cs.seg_y = 6;
        cs.sag = 0.1f;
        cs.wrinkle = cod_rng_range(rng, 0.04f, 0.065f);
        cs.twist = 0.1f;
        cs.fray = 0.012f;
        cs.seed = cw_cloth_seed(rng);
        CwGeoId cloth = cwa_cloth(A, cs);
        u32 key = cw_pick_fabric3(rng);
        cwa_add(A, key, cloth, LL(IDENT, a.x + (b.x - a.x) * t, a.y - 0.35f, a.z + (b.z - a.z) * t, cw_ry_of(pm) + CW_PI_F / 2, 1, 1, 1), CW_M(0.35f, 0.6f, 0.2f));
      }
    }
  }

  for (u32 bi = 0; bi < info->balconies.len; bi++) {
    const CwBalconyInfo *bal = &info->balconies.items[bi];
    m4 pm = bal->pm;
    i32 n = cod_rng_int(rng, 1, 4);
    for (i32 i = 0; i < n; i++) {
      f32 lx = bal->x + cod_rng_range(rng, -bal->w / 2 + 0.3f, bal->w / 2 - 0.3f);
      f32 lz = -cod_rng_range(rng, 0.35f, bal->d - 0.3f);
      v3 wp = cw_world_of(pm, lx, bal->y + 0.13f, lz);
      u32 id = CW_PICK(rng, CW_L_BALCONY);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.85f, 1.1f);
      f32 gg = cod_rng_range(rng, 1.0f, 1.4f);
      cwa_put_g(A, id, wp.x, wp.y, wp.z, ry, sc, gg);
    }
    if (cod_rng_f64(rng) < 0.55) {
      f32 cw_ = cod_rng_range(rng, 0.8f, 1.4f);
      f32 ch = cod_rng_range(rng, 0.7f, 1.1f);
      CwClothSpec cs = cw_cloth_defaults(cw_, ch);
      cs.seg_x = 7;
      cs.seg_y = 7;
      cs.sag = 0.09f;
      cs.wrinkle = cod_rng_range(rng, 0.04f, 0.07f);
      cs.thickness = 0.0034f;
      cs.fray = cod_rng_range(rng, 0.012f, 0.03f);
      cs.seed = cw_cloth_seed(rng);
      CwGeoId cloth = cwa_cloth(A, cs);
      f32 ox = cod_rng_range(rng, -0.3f, 0.3f);
      v3 wp = cw_world_of(pm, bal->x + ox, bal->y + 0.95f, -bal->d - 0.03f);
      u32 key = cw_pick_fabric3(rng);
      cwa_add(A, key, cloth, LL(IDENT, wp.x, wp.y, wp.z, cw_ry_of(pm) + CW_PI_F, 1, 1, 1), CW_M(0.4f, 0.55f, 0.2f));
    }
  }

  for (u32 ai = 0; ai < info->awnings.len; ai++) {
    const CwAwningInfo *aw = &info->awnings.items[ai];
    if (cod_rng_f64(rng) < 0.55) {
      v3 wp = cw_world_of(aw->pm, aw->x, aw->y + 1.0f, -0.16f);
      f32 gm = cod_rng_range(rng, 0.8f, 1.3f);
      cwa_put_s_m(A, CWP_SIGN_BOARD, wp.x, wp.y, wp.z, cw_ry_of(aw->pm) + CW_PI_F, m_minf(1.3f, aw->w / 1.6f), 1.0f, 1.0f, 1.0f, gm, 1.0f, 0.0f, 0.0f);
    }
  }
  for (u32 di = 0; di < info->doors.len; di++) {
    const CwDoorInfo *dr = &info->doors.items[di];
    if (cod_rng_f64(rng) < 0.5) {
      f32 ox = cod_rng_range(rng, -0.2f, 0.2f);
      v3 wp = cw_world_of(dr->pm, dr->x + ox, 2.55f, -0.12f);
      f32 sc = cod_rng_range(rng, 0.85f, 1.15f);
      cwa_put_g(A, CWP_SIGN_HANG, wp.x, wp.y, wp.z, cw_ry_of(dr->pm) + CW_PI_F, sc, 1.2f);
    }
    v3 wp = cw_world_of(dr->pm, dr->x, 0.02f, -0.55f);
    for (i32 i = 0; i < cod_rng_int(rng, 1, 4); i++) {
      f32 ox = wp.x + cod_rng_range(rng, -1.3f, 1.3f);
      f32 oz = wp.z + cod_rng_range(rng, -1.0f, 1.0f);
      if (!cw_is_open(ox, oz, 0.15f))
        continue;
      u32 id = CW_PICK(rng, CW_L_DOORSTEP);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.85f, 1.1f);
      f32 gg = cod_rng_range(rng, 1.0f, 1.4f);
      cwa_put_g(A, id, ox, cw_ground_y(ox, oz), oz, ry, sc, gg);
    }
  }

  // roof clutter: roofs are playable ground in this map
  i32 rp = (i32)m_roundf((f32)(spec->roof_props ? spec->roof_props : 2) * 2.4f) + 2;
  const CwFloor *rs = &info->roof_spec;
  f32 rx0 = rs->x - rs->w / 2 + 1.0f, rx1 = rs->x + rs->w / 2 - 1.0f;
  f32 rz0 = rs->z - rs->d / 2 + 1.0f, rz1 = rs->z + rs->d / 2 - 1.0f;
  f32 roofY = top + 0.02f;
  for (i32 i = 0; i < rp; i++) {
    f32 px = cod_rng_range(rng, rx0, rx1);
    f32 pz = cod_rng_range(rng, rz0, rz1);
    f32 pick = cod_rng_float(rng);
    if (pick < 0.22f) {
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.9f, 1.15f);
      f32 gg = cod_rng_range(rng, 0.9f, 1.3f);
      cwa_put_g(A, CWP_WATER_TANK, px, roofY, pz, ry, sc, gg);
      cwa_box(A, COD_SURFACE_METAL, px, roofY + 0.55f, pz, 1.2f, 1.1f, 1.2f, 0.0f);
    } else if (pick < 0.45f) {
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.85f, 1.15f);
      f32 gg = cod_rng_range(rng, 0.8f, 1.3f);
      cwa_put_g(A, CWP_SAT_DISH, px, roofY, pz, ry, sc, gg);
    } else if (pick < 0.6f) {
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      cwa_put_g(A, CWP_ROOF_VENT, px, roofY, pz, ry, 1.0f, 1.2f);
    } else if (pick < 0.78f) {
      i32 n = cod_rng_int(rng, 2, 4);
      for (i32 k = 0; k < n; k++) {
        u32 id = CW_PICK(rng, CW_L_ROOF_CRATE);
        f32 jx = cod_rng_range(rng, -0.15f, 0.15f);
        f32 jz = cod_rng_range(rng, -0.15f, 0.15f);
        f32 ry = cod_rng_float(rng) * CW_TAU32;
        f32 gg = cod_rng_range(rng, 1.0f, 1.4f);
        cwa_put_g(A, id, px + jx, roofY + (f32)k * 0.53f, pz + jz, ry, 1.0f, gg);
      }
      cwa_box(A, COD_SURFACE_WOOD, px, roofY + (f32)n * 0.26f, pz, 0.7f, (f32)n * 0.53f, 0.7f, 0.0f);
    } else {
      u32 id = CW_PICK(rng, CW_L_ROOF_MISC);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      f32 gg = cod_rng_range(rng, 1.1f, 1.5f);
      cwa_put_g(A, id, px, roofY, pz, ry, 1.0f, gg);
    }
  }
  for (i32 i = 0; i < 4; i++) {
    f32 prad = cod_rng_range(rng, 0.6f, 1.6f);
    CwGeoId g = cwa_patch(A, rng, prad, 9, 0.5f);
    f32 px = cod_rng_range(rng, rx0, rx1);
    f32 pz = cod_rng_range(rng, rz0, rz1);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    cwa_add(A, CW_DIRT, g, LL(IDENT, px, roofY + 0.012f, pz, ry, 1, 1, 0.7f), CW_M(0.1f, 0.85f, 0.5f));
  }
  for (i32 i = 0; i < cod_rng_int(rng, 4, 10); i++) {
    f32 px = cod_rng_range(rng, rx0 + 0.7f, rx1 - 0.7f);
    f32 pz = cod_rng_range(rng, rz0 + 0.7f, rz1 - 0.7f);
    u32 id = CW_PICK(rng, CW_L_ROOF_JUNK);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sc = cod_rng_range(rng, 0.6f, 1.2f);
    cwa_put_g(A, id, px, roofY + 0.02f, pz, ry, sc, 1.4f);
  }
  // rooftop laundry line between the parapets
  if (rs->w > 10 && cod_rng_f64(rng) < 0.4) {
    f32 az = cod_rng_range(rng, rz0, rz1);
    f32 bz = cod_rng_range(rng, rz0, rz1);
    v3 a = {rs->x - rs->w / 2 + 0.4f, roofY + 1.0f, az};
    v3 b = {rs->x + rs->w / 2 - 0.4f, roofY + 0.96f, bz};
    CwGeoId line = cwa_catenary(A, a, b, 0.3f, 0.01f, 10, 4, 0.0f);
    cwa_add(A, CW_METAL_DARK, line, IDENT, CW_M(0.3f, 0.6f, 0.0f));
    for (i32 sx = -1; sx <= 1; sx += 2)
      cwa_add(A, CW_METAL_RUST, BOX_FINE(A), LL(IDENT, rs->x + (f32)sx * (rs->w / 2 - 0.4f), roofY + 0.9f, a.z + (sx > 0 ? b.z - a.z : 0.0f), 0, 0.06f, 1.8f, 0.06f), CW_M(0.9f, 0.5f, 0.1f));
    i32 n = cod_rng_int(rng, 2, 5);
    for (i32 i = 0; i < n; i++) {
      f32 t = ((f32)i + 0.5f) / (f32)n;
      f32 cw_ = cod_rng_range(rng, 0.5f, 0.8f);
      f32 ch = cod_rng_range(rng, 0.45f, 0.8f);
      CwClothSpec cs = cw_cloth_defaults(cw_, ch);
      cs.seg_x = 7;
      cs.seg_y = 8;
      cs.sag = cod_rng_range(rng, 0.12f, 0.22f);
      cs.wrinkle = cod_rng_range(rng, 0.045f, 0.075f);
      cs.twist = cod_rng_range(rng, 0.08f, 0.18f);
      cs.fray = cod_rng_range(rng, 0.01f, 0.025f);
      cs.seed = cw_cloth_seed(rng);
      CwGeoId cloth = cwa_cloth(A, cs);
      u32 key = CW_PICK(rng, CW_L_FABRIC4);
      f32 gm = cod_rng_range(rng, 0.4f, 0.8f);
      cwa_add(A, key, cloth, LL(IDENT, a.x + (b.x - a.x) * t, a.y - 0.5f - 0.22f * m_sinf(t * CW_PI_F), a.z + (b.z - a.z) * t, 0, 1, 1, 1), CW_M(0.3f, gm, 0.2f));
    }
  }
  if (cod_rng_f64(rng) < 0.6) {
    f32 mx = cod_rng_range(rng, rx0, rx1);
    f32 mz = cod_rng_range(rng, rz0, rz1);
    f32 mr = cod_rng_range(rng, 0.7f, 1.3f);
    i32 mc = cod_rng_int(rng, 8, 16);
    cw_rubble_mound(A, rng, mx, roofY, mz, mr, (u32)mc, CW_CONCRETE_DARK);
  }
  // aerials: thin, tall, and they do a lot for a skyline
  for (i32 i = 0; i < cod_rng_int(rng, 1, 3); i++) {
    f32 px = cod_rng_range(rng, rx0, rx1);
    f32 pz = cod_rng_range(rng, rz0, rz1);
    f32 h = cod_rng_range(rng, 1.4f, 3.2f);
    CwGeoId pipe;
    if (!cwa_cache_find(A, CWK_AERIAL, 0, 0, &pipe))
      pipe = cwa_cache_store(A, CWK_AERIAL, 0, 0, cwa_ready(A, cw_tube_y(&A->al, 0.018f, 1.0f, 5, 1.0f, false, 1)));
    cwa_add(A, CW_METAL_RUST, pipe, LL(IDENT, px, roofY, pz, 0, 1, h, 1), CW_M(0.9f, 0.5f, 0.1f));
    for (i32 k = 0; k < 4; k++) {
      f32 ry = cod_rng_float(rng) * 3.14f;
      f32 sy = cod_rng_range(rng, 0.25f, 0.5f);
      cwa_add(A, CW_METAL_RUST, pipe, LLR(IDENT, px, roofY + h * (0.5f + (f32)k * 0.11f), pz, ry, 1, sy, 1, 0, CW_PI_F / 2), CW_M(0.9f, 0.5f, 0.1f));
    }
  }
}

// cables and washing lines strung across the alleys between buildings
hz_internal void cw_alley_lines(CwA *A, CodRng *rng) {
  static const f32 spans[6][6] = {{-6.6f, 5.0f, 21.0f, -6.6f, 5.4f, 24.0f}, {-6.6f, 4.2f, -9.0f, -6.6f, 4.6f, -11.5f},
                                  {7.0f, 4.6f, 2.5f, 7.0f, 4.2f, 6.6f},     {7.0f, 5.6f, -16.0f, 7.0f, 5.2f, -20.0f},
                                  {-8.0f, 6.4f, 20.6f, -8.0f, 6.0f, 23.8f}, {8.6f, 6.2f, 2.2f, 8.6f, 5.8f, 7.2f}};
  for (u32 si = 0; si < 6; si++) {
    f32 x0 = spans[si][0], y0 = spans[si][1], z0 = spans[si][2], x1 = spans[si][3], y1 = spans[si][4], z1 = spans[si][5];
    CwGeoId t = cwa_catenary(A, (v3){x0, y0, z0}, (v3){x1, y1, z1}, 0.5f, 0.016f, 10, 4, 0.04f);
    cwa_add(A, CW_METAL_DARK, t, IDENT, CW_M(0.4f, 0.7f, 0.2f));
    i32 n = cod_rng_int(rng, 2, 4);
    for (i32 i = 0; i < n; i++) {
      f32 f = ((f32)i + 0.5f) / (f32)n;
      f32 cw_ = cod_rng_range(rng, 0.45f, 0.8f);
      f32 ch = cod_rng_range(rng, 0.5f, 1.0f);
      CwClothSpec cs = cw_cloth_defaults(cw_, ch);
      cs.seg_x = 6;
      cs.seg_y = 8;
      cs.sag = cod_rng_range(rng, 0.12f, 0.22f);
      cs.wrinkle = cod_rng_range(rng, 0.045f, 0.075f);
      cs.twist = cod_rng_range(rng, 0.08f, 0.18f);
      cs.fray = cod_rng_range(rng, 0.01f, 0.025f);
      cs.seed = cw_cloth_seed(rng);
      CwGeoId cloth = cwa_cloth(A, cs);
      u32 key = CW_PICK(rng, CW_L_FABRIC4);
      f32 gm = cod_rng_range(rng, 0.4f, 0.8f);
      cwa_add(A, key, cloth, LL(IDENT, x0 + (x1 - x0) * f, y0 + (y1 - y0) * f - 0.6f - 0.4f * m_sinf(f * CW_PI_F), z0 + (z1 - z0) * f, m_atan2f(-(z1 - z0), x1 - x0), 1, 1, 1),
              CW_M(0.3f, gm, 0.2f));
    }
  }
}

hz_internal void cw_dress_buildings(CwA *A, CodRng *rng, CwBuildInfo **infos, u32 count) {
  cwa_jitter_on(A);
  for (u32 i = 0; i < count; i++)
    cw_dress_building(A, rng, infos[i]);
  cw_alley_lines(A, rng);
  cwa_jitter_off(A);
}

// ----------------------------------------------------------------- scatter --

hz_internal u32 cw_scatter_wall_id(CodRng *rng, f32 pick) {
  if (pick < 0.3f) return CWP_LITTER;
  if (pick < 0.46f) return CW_PICK(rng, CW_L_BRICK2);
  if (pick < 0.58f) return CW_PICK(rng, CW_L_ROCK2);
  if (pick < 0.68f) return CWP_WEEDS;
  if (pick < 0.76f) return CW_PICK(rng, CW_L_CAN_BOTTLE);
  if (pick < 0.84f) return CW_PICK(rng, CW_L_PLANK2);
  if (pick < 0.9f) return CWP_CINDER;
  if (pick < 0.95f) return CW_PICK(rng, CW_L_CARD2);
  return CW_PICK(rng, CW_L_SCATTER_MISC);
}

hz_internal u32 cw_scatter_alley_id(CodRng *rng, f32 pick) {
  if (pick < 0.2f) return CWP_LITTER;
  if (pick < 0.34f) return CW_PICK(rng, CW_L_ALLEY_BRICK);
  if (pick < 0.46f) return CW_PICK(rng, CW_L_ROCK2);
  if (pick < 0.56f) return CWP_WEEDS;
  if (pick < 0.64f) return CWP_SHRUB;
  if (pick < 0.72f) return CW_PICK(rng, CW_L_PLANK2);
  if (pick < 0.8f) return CW_PICK(rng, CW_L_ALLEY_CRATE);
  if (pick < 0.86f) return CW_PICK(rng, CW_L_ALLEY_BARREL);
  if (pick < 0.9f) return CW_PICK(rng, CW_L_TYRE2);
  if (pick < 0.95f) return CW_PICK(rng, CW_L_ALLEY_MISC);
  return CW_PICK(rng, CW_L_ALLEY_HEAVY);
}

hz_internal void cw_scatter_debris(CwA *A, CodRng *rng) {
  const f32 zMin = CW_STREET_ZMIN, zMax = CW_STREET_ZMAX, kerb = CW_STREET_KERB, HW = CW_STREET_HW;
  cwa_jitter_on(A);

  for (i32 i = 0; i < 340; i++) {
    f32 side = cod_rng_f64(rng) < 0.5 ? -1.0f : 1.0f;
    f32 z = cod_rng_range(rng, zMin + 1, zMax - 1);
    f32 off = 0.12f + m_fabsf(cod_rng_gauss(rng)) * 0.75f;
    f32 x = side * (kerb - off);
    if (!cw_is_open(x, z, 0.05f))
      continue;
    f32 y = cw_ground_y(x, z);
    f32 pick = cod_rng_float(rng);
    u32 id = cw_scatter_wall_id(rng, pick);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sc = cod_rng_range(rng, 0.65f, 1.25f);
    f32 gg = cod_rng_range(rng, 1.0f, 1.5f);
    cwa_put_g(A, id, x, y + 0.015f, z, ry, sc, gg);
  }

  for (i32 i = 0; i < 180; i++) {
    f32 xr = cod_rng_range(rng, -HW + 0.1f, HW - 0.1f);
    f32 sg = cod_rng_signed(rng);
    f32 x = xr * (0.45f + 0.55f * m_fabsf(sg));
    f32 z = cod_rng_range(rng, zMin + 1, zMax - 1);
    if (!cw_is_open(x, z, 0.05f))
      continue;
    u32 id = CW_PICK(rng, CW_L_ROAD_JUNK);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sc = cod_rng_range(rng, 0.6f, 1.15f);
    f32 gg = cod_rng_range(rng, 1.0f, 1.5f);
    cwa_put_g(A, id, x, cw_ground_y(x, z) + 0.012f, z, ry, sc, gg);
  }

  for (u32 ai = 0; ai < CW_ALLEY_COUNT; ai++) {
    const CwAlley *a = &CW_ALLEYS[ai];
    f32 x0 = a->x0, z0 = a->z0, x1 = a->x1, z1 = a->z1;
    f32 area = (x1 - x0) * (z1 - z0);
    i32 n = (i32)m_roundf(area * 0.85f);
    for (i32 i = 0; i < n; i++) {
      f32 x = cod_rng_range(rng, x0 + 0.3f, x1 - 0.3f);
      f32 z = cod_rng_range(rng, z0 + 0.3f, z1 - 0.3f);
      if (cw_in_building(x, z, 0.25f))
        continue;
      f32 nearw = cw_nearest_wall(x, z);
      f32 wallBias = nearw < 1.2f ? 1.0f : 0.45f;
      if (cod_rng_f64(rng) > (f64)wallBias)
        continue;
      f32 pick = cod_rng_float(rng);
      u32 id = cw_scatter_alley_id(rng, pick);
      f32 y = cw_ground_y(x, z);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.7f, 1.2f);
      f32 gg = cod_rng_range(rng, 1.0f, 1.5f);
      cwa_put_g(A, id, x, y + 0.015f, z, ry, sc, gg);
      if (id == CWP_BARREL_RUST || id == CWP_BARREL_BLUE || id == CWP_BARREL_WOOD)
        cwa_box(A, COD_SURFACE_METAL, x, y + 0.45f, z, 0.62f, 0.9f, 0.62f, 0.0f);
      else if (id == CWP_CRATE_A || id == CWP_CRATE_B || id == CWP_CRATE_C || id == CWP_CRATE_FLAT)
        cwa_box(A, COD_SURFACE_WOOD, x, y + 0.3f, z, 0.62f, 0.6f, 0.62f, 0.0f);
    }
    if (cod_rng_f64(rng) < 0.7) {
      f32 bx = cod_rng_f64(rng) < 0.5 ? x0 + 1.6f : x1 - 1.6f;
      f32 bz = cod_rng_range(rng, z0 + 1.2f, z1 - 1.2f);
      if (!cw_in_building(bx, bz, 0.4f)) {
        f32 mr = cod_rng_range(rng, 0.9f, 1.8f);
        i32 mc = cod_rng_int(rng, 12, 24);
        cw_rubble_mound(A, rng, bx, cw_ground_y(bx, bz), bz, mr, (u32)mc, CW_CONCRETE);
      }
    }
  }

  for (i32 i = 0; i < 220; i++) {
    f32 side = cod_rng_f64(rng) < 0.5 ? -1.0f : 1.0f;
    f32 z = cod_rng_range(rng, zMin + 1, zMax - 1);
    b32 atKerb = cod_rng_f64(rng) < 0.55;
    f32 x = atKerb ? side * (HW + cod_rng_range(rng, 0.02f, 0.3f)) : side * (kerb - cod_rng_range(rng, 0.05f, 0.35f));
    if (!cw_is_open(x, z, 0.02f))
      continue;
    u32 id = cod_rng_f64(rng) < 0.78 ? CWP_WEEDS : CWP_SHRUB;
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sc = cod_rng_range(rng, 0.6f, 1.25f);
    f32 gg = cod_rng_range(rng, 1.0f, 1.4f);
    cwa_put_g(A, id, x, cw_ground_y(x, z) + 0.01f, z, ry, sc, gg);
  }

  for (i32 i = 0; i < 60; i++) {
    f32 side = cod_rng_f64(rng) < 0.5 ? -1.0f : 1.0f;
    f32 z = cod_rng_range(rng, zMin + 2, zMax - 2);
    f32 x = side * (kerb - cod_rng_range(rng, 0.1f, 0.5f));
    if (!cw_is_open(x, z, 0.05f))
      continue;
    f32 prad = cod_rng_range(rng, 0.3f, 0.8f);
    CwGeoId g = cwa_patch(A, rng, prad, 8, 0.6f);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    cwa_add(A, CW_DIRT, g, LL(IDENT, x, cw_ground_y(x, z) + 0.01f, z, ry, 1, 1, 0.6f), CW_M(0.1f, 0.95f, 0.7f));
    for (i32 k = 0; k < cod_rng_int(rng, 2, 6); k++) {
      f32 px = x + cod_rng_range(rng, -0.5f, 0.5f);
      f32 pz = z + cod_rng_range(rng, -0.6f, 0.6f);
      f32 lry = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.7f, 1.2f);
      cwa_put_g(A, CWP_LITTER, px, cw_ground_y(x, z) + 0.02f, pz, lry, sc, 1.5f);
    }
  }
  cwa_jitter_off(A);
}

// ---------------------------------------------------------------------- gate --

// a deep opening in the terminator mass: recessed void, reveal returns, a stone head and sill
hz_internal void cw_gate_aperture(CwA *A, CodRng *rng, f32 x, f32 y, f32 z, f32 w, f32 h, f32 t, f32 recess, b32 sill) {
  f32 zf = z + t / 2;
  f32 rec = recess > 0.0f ? recess : 0.5f;
  cwa_add(A, CW_WINDOW_VOID, BOX(A), LL(IDENT, x, y, zf - rec - 0.06f, 0, w, h, 0.12f), CW_M(0.15f, 0.95f, 1.0f));
  cwa_add(A, CW_CONCRETE_DARK, BOX(A), LL(IDENT, x, y + h / 2 + 0.07f, zf - rec / 2, 0, w + 0.3f, 0.14f, rec), CW_M(0.3f, 0.85f, 0.9f));
  cwa_add(A, CW_CONCRETE_DARK, BOX(A), LL(IDENT, x, y - h / 2 - 0.07f, zf - rec / 2, 0, w + 0.3f, 0.14f, rec), CW_M(0.55f, 0.75f, 0.7f));
  for (i32 s = -1; s <= 1; s += 2)
    cwa_add(A, CW_CONCRETE_DARK, BOX(A), LL(IDENT, x + (f32)s * (w / 2 + 0.07f), y, zf - rec / 2, 0, 0.14f, h, rec), CW_M(0.3f, 0.8f, 0.85f));
  cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(IDENT, x, y + h / 2 + 0.16f, zf + 0.09f, 0, w + 0.5f, 0.2f, 0.34f), CW_M(0.7f, 0.5f, 0.25f));
  if (sill)
    cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(IDENT, x, y - h / 2 - 0.1f, zf + 0.12f, 0, w + 0.44f, 0.11f, 0.42f), CW_M(0.55f, 0.45f, 0.3f));
  if (cod_rng_f64(rng) < 0.4) {
    f32 jx = cod_rng_range(rng, -0.1f, 0.1f);
    f32 sw = w * cod_rng_range(rng, 0.5f, 0.9f);
    f32 sh = h * cod_rng_range(rng, 0.4f, 0.8f);
    cwa_add(A, CW_METAL_RUST, BOX(A), LL(IDENT, x + jx, y - h * 0.1f, zf - 0.14f, 0, sw, sh, 0.03f), CW_M(0.9f, 0.6f, 0.2f));
  }
}

// an irregular crenellated run: varied widths, gaps, heights, leans and sheared-off merlons
hz_internal void cw_merlon_run(CwA *A, CodRng *rng, f32 x0, f32 x1, f32 z, f32 t, f32 yTop, u32 key, f32 depth, f32 set) {
  f32 dt = t * (depth > 0.0f ? depth : 0.45f);
  f32 zc = z + t / 2 - dt / 2 - (set > 0.0f ? set : 0.06f);
  cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(IDENT, (x0 + x1) / 2, yTop + 0.07f, z, 0, x1 - x0, 0.14f, t + 0.3f), CW_M(0.85f, 0.4f, 0.15f));
  f32 x = x0 + cod_rng_range(rng, 0.05f, 0.35f);
  while (x < x1 - 0.4f) {
    f32 w = m_minf(cod_rng_range(rng, 0.62f, 1.35f), x1 - 0.1f - x);
    if (w < 0.3f)
      break;
    b32 broken = cod_rng_f64(rng) < 0.22;
    f32 h = broken ? cod_rng_range(rng, 0.16f, 0.42f) : cod_rng_range(rng, 0.62f, 1.15f);
    f32 cx = x + w / 2;
    f32 lean = cod_rng_range(rng, -0.035f, 0.035f);
    cwa_add(A, key, BOX(A), LLR(IDENT, cx, yTop + 0.14f + h / 2, zc, 0, w, h, dt, 0, lean), CW_M(0.55f, 0.45f, 0.2f));
    cwa_box(A, COD_SURFACE_CONCRETE, cx, yTop + 0.14f + h / 2, zc, w, h, dt, 0.0f);
    if (!broken && cod_rng_f64(rng) < 0.55)
      cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(IDENT, cx, yTop + 0.16f + h, zc, 0, w + 0.1f, 0.07f, dt + 0.1f), CW_M(0.9f, 0.35f, 0.1f));
    if (cod_rng_f64(rng) < 0.45) {
      f32 sw = w * cod_rng_range(rng, 0.3f, 0.62f);
      f32 sh = h * cod_rng_range(rng, 0.25f, 0.55f);
      CwGeoId g = cwa_spall(A, sw, sh, 0.02f);
      f32 ox = cod_rng_range(rng, -w * 0.2f, w * 0.2f);
      f32 oy = h * cod_rng_range(rng, 0.3f, 0.7f);
      cwa_add(A, CW_BRICK_FINE, g, LL(IDENT, cx + ox, yTop + 0.14f + oy, zc + dt / 2 - 0.013f, 0, 1, 1, 1), NULL);
    }
    x += w + cod_rng_range(rng, 0.34f, 0.95f);
  }
}

// one block of the terminator mass: body, plinth, pilasters, cornice, corbels, string course, spalled render
hz_internal void cw_gate_block(CwA *A, CodRng *rng, f32 x0, f32 x1, f32 h, f32 tt, f32 zc, u32 key) {
  f32 cx = (x0 + x1) / 2, w = x1 - x0;
  cwa_add(A, key, BOX(A), LL(IDENT, cx, h / 2, zc, 0, w, h, tt), CW_M(0.45f, 0.6f, 0.35f));
  cwa_box(A, COD_SURFACE_CONCRETE, cx, h / 2, zc, w, h, tt, 0.0f);
  cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(IDENT, cx, 0.4f, zc, 0, w + 0.24f, 0.8f, tt + 0.26f), CW_M(0.6f, 0.85f, 0.55f));
  for (i32 s = -1; s <= 1; s += 2)
    cwa_add(A, key, BOX(A), LL(IDENT, cx + (f32)s * (w / 2 - 0.3f), h * 0.5f, zc + tt / 2 + 0.15f, 0, 0.6f, h - 0.2f, 0.34f), CW_M(0.6f, 0.5f, 0.25f));
  cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(IDENT, cx, h - 0.22f, zc + 0.2f, 0, w + 0.5f, 0.3f, tt + 0.66f), CW_M(0.8f, 0.45f, 0.2f));
  i32 nb = (i32)m_maxf(2.0f, m_roundf(w / 1.15f));
  for (i32 i = 0; i < nb; i++) {
    f32 bx = x0 + 0.35f + ((f32)i / m_maxf(1.0f, (f32)(nb - 1))) * (w - 0.7f);
    cwa_add(A, CW_CONCRETE, BOX(A), LL(IDENT, bx, h - 0.62f, zc + tt / 2 + 0.22f, 0, 0.22f, 0.44f, 0.46f), CW_M(0.7f, 0.55f, 0.35f));
  }
  cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(IDENT, cx, h * 0.46f, zc + tt / 2 + 0.11f, 0, w + 0.18f, 0.16f, 0.3f), CW_M(0.8f, 0.5f, 0.25f));
  i32 sp = (i32)m_roundf(w * h * 0.05f);
  for (i32 i = 0; i < sp; i++) {
    f32 sw = cod_rng_range(rng, 0.24f, 0.7f);
    f32 sh = cod_rng_range(rng, 0.22f, 0.6f);
    CwGeoId g = cwa_spall(A, sw, sh, 0.022f);
    f32 px = cod_rng_range(rng, x0 + 0.5f, x1 - 0.5f);
    f32 py = cod_rng_range(rng, 0.9f, h - 0.7f);
    cwa_add(A, CW_BRICK_FINE, g, LL(IDENT, px, py, zc + tt / 2 - 0.014f, 0, 1, 1, 1), NULL);
  }
}

hz_internal void cw_build_gate(CwA *A, CodRng *rng) {
  const f32 z = CW_GATE_Z, t = CW_GATE_DEPTH, span = CW_GATE_SPAN, height = CW_GATE_HEIGHT, outerW = CW_GATE_OUTER_W, bodyH = CW_GATE_BODY_H;
  const f32 xL0 = CW_GATE_XL0, xL1 = CW_GATE_XL1, hL = CW_GATE_HL, xR0 = CW_GATE_XR0, xR1 = CW_GATE_XR1, hR = CW_GATE_HR, eastProud = CW_GATE_EAST_PROUD;
  const f32 xT0 = CW_GATE_XT0, xT1 = CW_GATE_XT1, hT = CW_GATE_HT, towerProud = CW_GATE_TOWER_PROUD;

  cw_gate_block(A, rng, xL0, xL1, hL, t, z, CW_PLASTER_SAND);
  for (i32 i = 0; i < 3; i++)
    cw_gate_aperture(A, rng, xL0 + 1.0f + (f32)i * ((xL1 - xL0 - 2.0f) / 2), hL * 0.66f, z, 0.9f, 1.5f, t, 0.0f, true);
  cw_gate_aperture(A, rng, (xL0 + xL1) / 2, hL * 0.3f, z, 1.1f, 1.3f, t, 0.0f, true);
  cw_merlon_run(A, rng, xL0, xL1, z, t, hL, CW_PLASTER_SAND, 0.0f, 0.0f);

  f32 zR = z + eastProud / 2, tR = t + eastProud;
  cw_gate_block(A, rng, xR0, xR1, hR, tR, zR, CW_PLASTER_BLUE);
  cw_gate_aperture(A, rng, (xR0 + xR1) / 2, hR * 0.62f, zR, 1.0f, 1.6f, tR, 0.0f, true);
  cw_gate_aperture(A, rng, (xR0 + xR1) / 2, hR * 0.34f, zR, 0.85f, 1.2f, tR, 0.0f, true);
  cw_merlon_run(A, rng, xR0, xR1, zR, tR, hR, CW_PLASTER_BLUE, 0.0f, 0.0f);

  f32 zT = z + towerProud / 2;
  cw_gate_block(A, rng, xT0, xT1, hT, t + towerProud, zT, CW_PLASTER_CREAM);
  for (i32 i = 0; i < 3; i++)
    cw_gate_aperture(A, rng, (xT0 + xT1) / 2 + (f32)(i - 1) * 1.05f, hT * 0.55f + (i == 1 ? 0.25f : 0.0f), zT, 0.5f, i == 1 ? 1.5f : 1.1f, t + towerProud, 0.0f, true);
  cw_gate_aperture(A, rng, (xT0 + xT1) / 2, hT * 0.8f, zT, 1.5f, 1.0f, t + towerProud, 0.75f, true);
  cw_merlon_run(A, rng, xT0, xT1, z + towerProud / 2, t + towerProud, hT, CW_PLASTER_CREAM, 0.0f, 0.0f);
  cwa_add(A, CW_METAL_RUST, BOX(A), LLR(IDENT, xT1 - 0.5f, hT + 1.9f, zT, 0, 0.06f, 3.4f, 0.06f, 0.04f, 0.07f), CW_M(0.95f, 0.5f, 0.0f));
  cwa_put_g(A, CWP_SAT_DISH, xT0 + 0.9f, hT + 0.3f, zT + 0.4f, 0.7f, 1.0f, 1.3f);

  cw_sandbag_wall(A, rng, xL0 + 1.9f, z - 0.15f, 0.0f, 2.4f, 3, true, hL + 0.16f);
  cw_sandbag_wall(A, rng, xR0 + 1.7f, zR - 0.15f, 0.0f, 1.9f, 3, true, hR + 0.16f);
  cw_sandbag_wall(A, rng, (xT0 + xT1) / 2, zT - 0.25f, 0.0f, 2.2f, 4, true, hT + 0.16f);
  A->skirts = false;
  cwa_put_g(A, CWP_CRATE_C, xL1 - 1.2f, hL + 0.16f, z - 0.6f, 0.4f, 1.0f, 1.3f);
  cwa_put_g(A, CWP_BARREL_RUST, xR0 + 0.6f, hR + 0.16f, zR - 0.5f, 0.2f, 1.0f, 1.4f);
  A->skirts = true;

  // the spandrel over the arch
  f32 spanH = bodyH - height;
  cwa_add(A, CW_PLASTER_SAND, BOX(A), LL(IDENT, 0, height + spanH / 2, z, 0, span + 0.4f, spanH, t), CW_M(0.45f, 0.6f, 0.35f));
  cwa_box(A, COD_SURFACE_CONCRETE, 0, height + spanH / 2, z, span + 0.4f, spanH, t, 0.0f);

  // arch voussoirs around a pointed profile
  const i32 seg = 15;
  for (i32 i = 0; i <= seg; i++) {
    f32 a = ((f32)i / (f32)seg) * CW_PI_F;
    f32 r = span / 2;
    f32 px = -m_cosf(a) * r;
    f32 py = height - r + m_sinf(a) * r * 1.18f;
    if (py < height - r - 0.01f)
      continue;
    f32 ang = a - CW_PI_F / 2;
    cwa_add(A, CW_CONCRETE, BOX(A), LLR(IDENT, px, py, z, 0, 0.62f, 0.42f, t + 0.14f, 0, -ang), CW_M(0.7f, 0.45f, 0.25f));
  }
  for (i32 sx = -1; sx <= 1; sx += 2) {
    f32 fx = (f32)sx;
    cwa_add(A, CW_CONCRETE, BOX(A), LL(IDENT, fx * (span / 2 + 0.1f), height - span / 2 - 0.2f, z, 0, 0.6f, 0.4f, t + 0.2f), CW_M(0.7f, 0.5f, 0.3f));
    cwa_box(A, COD_SURFACE_CONCRETE, fx * (span / 2 + 0.28f), (height - span / 2) / 2, z, 0.56f, height - span / 2, t + 0.2f, 0.0f);
  }

  // the rampart walkway over the arch, on corbels
  f32 wz = z + t / 2 + 0.38f;
  cwa_add(A, CW_ROOF_SCREED, BOX(A), LL(IDENT, 0, bodyH + 0.11f, wz, 0, span + 1.4f, 0.22f, 0.82f), CW_M(0.55f, 0.35f, 0.15f));
  cwa_box(A, COD_SURFACE_CONCRETE, 0, bodyH + 0.11f, wz, span + 1.4f, 0.22f, 0.82f, 0.0f);
  for (i32 i = 0; i < 6; i++) {
    f32 bx = -(span + 0.6f) / 2 + ((f32)i / 5.0f) * (span + 0.6f);
    cwa_add(A, CW_CONCRETE, BOX(A), LL(IDENT, bx, bodyH - 0.24f, wz - 0.06f, 0, 0.2f, 0.46f, 0.66f), CW_M(0.7f, 0.6f, 0.4f));
  }
  cw_merlon_run(A, rng, -span / 2 - 0.6f, span / 2 + 0.6f, z + 0.76f, t, bodyH + 0.22f, CW_PLASTER_SAND, 0.34f, 0.02f);
  cw_sandbag_wall(A, rng, -0.9f, z + 0.15f, 0.0f, 2.0f, 3, true, bodyH + 0.34f);

  // guard hut and checkpoint clutter under the arch
  cwa_put_g(A, CWP_BLOCK_BIG, 0.0f, 0.0f, z + 3.2f, 0.1f, 1.0f, 1.2f);
  cwa_box(A, COD_SURFACE_CONCRETE, 0, 0.48f, z + 3.2f, 1.3f, 0.96f, 0.9f, 0.0f);
  static const f32 jer[4][3] = {{-2.2f, 2.6f, 0.1f}, {2.4f, 2.2f, 1.6f}, {-1.4f, -2.4f, 1.5f}, {2.0f, -2.8f, 0.2f}};
  for (u32 i = 0; i < 4; i++) {
    f32 bx = jer[i][0], bz = z + jer[i][1], br = jer[i][2];
    f32 gg = cod_rng_range(rng, 0.9f, 1.3f);
    cwa_put_g(A, CWP_JERSEY, bx, 0.0f, bz, br, 1.0f, gg);
    cwa_box(A, COD_SURFACE_CONCRETE, bx, 0.46f, bz, 0.62f, 0.92f, 1.9f, br);
  }
  cw_sandbag_wall(A, rng, -1.9f, z + 4.6f, 0.1f, 2.4f, 4, false, 0.0f);
  cw_sandbag_wall(A, rng, 2.1f, z - 4.4f, 0.0f, 2.0f, 3, false, 0.0f);
  for (i32 i = 0; i < 24; i++) {
    f32 px = cod_rng_range(rng, -outerW / 2, outerW / 2);
    f32 pz = z + cod_rng_range(rng, -5.0f, 5.0f);
    if (m_fabsf(px) > span / 2 && m_fabsf(pz - z) < t / 2 + 0.3f)
      continue;
    u32 id = CW_PICK(rng, CW_L_GATE_JUNK);
    f32 ry = cod_rng_float(rng) * CW_TAU32;
    f32 sc = cod_rng_range(rng, 0.6f, 1.2f);
    cwa_put_g(A, id, px, cw_ground_y(px, pz) + 0.02f, pz, ry, sc, 1.4f);
  }
  cw_rubble_mound(A, rng, -span / 2 - 1.0f, 0.0f, z + 1.4f, 1.2f, 16, CW_CONCRETE);
  cw_rubble_mound(A, rng, span / 2 + 1.4f, 0.0f, z - 1.6f, 1.0f, 12, CW_CONCRETE);

  // bullet scarring, clustered; kept off the tower's proud face for the first set
  if (cwa_has(A, CWP_POCK)) {
    for (i32 b = 0; b < 12; b++) {
      f32 cx = cod_rng_range(rng, xL0 + 0.5f, xR1 - 0.5f);
      f32 cy = cod_rng_range(rng, 0.6f, 6.0f);
      if (m_fabsf(cx) < span / 2 && cy < height)
        continue;
      for (i32 j = 0; j < cod_rng_int(rng, 3, 8); j++) {
        f32 px = cx + cod_rng_gauss(rng) * 0.4f;
        f32 py = cy + cod_rng_gauss(rng) * 0.3f;
        if (m_fabsf(px) < span / 2 && py < height)
          continue;
        if (px < xL0 + 0.1f || px > xR1 - 0.1f || py < 0.2f)
          continue;
        if (py > (px < xL1 ? hL : hR) - 0.4f)
          continue;
        f32 s = cod_rng_range(rng, 0.55f, 1.4f);
        f32 sz = cod_rng_range(rng, 0.5f, 1.2f);
        f32 gg = cod_rng_range(rng, 0.7f, 1.3f);
        cwa_put_s_m(A, CWP_POCK, px, py, z + t / 2 + 0.0015f, 0, s, s, sz, 1.0f, gg, 1.0f, 0.0f, 0.0f);
      }
    }
    for (i32 b = 0; b < 4; b++) {
      f32 cx = cod_rng_range(rng, xT0 + 0.4f, xT1 - 0.4f);
      f32 cy = cod_rng_range(rng, 0.8f, hT - 1.0f);
      for (i32 j = 0; j < cod_rng_int(rng, 3, 7); j++) {
        f32 px = cx + cod_rng_gauss(rng) * 0.35f;
        f32 py = cy + cod_rng_gauss(rng) * 0.28f;
        if (px < xT0 + 0.1f || px > xT1 - 0.1f || py < 0.3f)
          continue;
        f32 s = cod_rng_range(rng, 0.5f, 1.3f);
        f32 sz = cod_rng_range(rng, 0.5f, 1.1f);
        f32 gg = cod_rng_range(rng, 0.7f, 1.3f);
        cwa_put_s_m(A, CWP_POCK, px, py, z + t / 2 + towerProud + 0.0015f, 0, s, s, sz, 1.0f, gg, 1.0f, 0.0f, 0.0f);
      }
    }
  }
}

// the map edge: compound walls, blocked cross-streets and rubble barricades
hz_internal void cw_build_perimeter(CwA *A, CodRng *rng) {
  const f32 R = 58.0f;
  const f32 segs[4][4] = {{-R, -R, R, -R}, {-R, R, R, R}, {-R, -R, -R, R}, {R, -R, R, R}};
  for (u32 si = 0; si < 4; si++) {
    f32 x0 = segs[si][0], z0 = segs[si][1], x1 = segs[si][2], z1 = segs[si][3];
    f32 dx = x1 - x0, dz = z1 - z0;
    f32 len = m_sqrtf(dx * dx + dz * dz);
    f32 ry = m_atan2f(dx, dz) - CW_PI_F / 2;
    i32 n = (i32)m_roundf(len / 4.0f);
    for (i32 i = 0; i < n; i++) {
      f32 t = ((f32)i + 0.5f) / (f32)n;
      f32 px = x0 + dx * t, pz = z0 + dz * t;
      f32 h = cod_rng_range(rng, 3.0f, 3.8f);
      u32 key = CW_PICK(rng, CW_L_PERIM_WALL);
      cwa_add(A, key, BOX(A), LL(IDENT, px, h / 2, pz, ry, len / (f32)n + 0.05f, h, 0.4f), CW_M(0.5f, 0.7f, 0.4f));
      cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(IDENT, px, h + 0.06f, pz, ry, len / (f32)n + 0.14f, 0.12f, 0.54f), CW_M(0.8f, 0.4f, 0.15f));
      cwa_box(A, COD_SURFACE_CONCRETE, px, h / 2, pz, len / (f32)n + 0.05f, h, 0.45f, ry);
    }
  }
  static const f32 blocks[2][2] = {{0.0f, CW_STREET_ZMAX + 1.5f}, {0.0f, CW_STREET_ZMIN - 1.5f}};
  for (u32 bi = 0; bi < 2; bi++) {
    f32 bx = blocks[bi][0], bz = blocks[bi][1];
    for (i32 i = -1; i <= 1; i++) {
      f32 jr = 0.02f + cod_rng_range(rng, -0.05f, 0.05f);
      cwa_put_g(A, CWP_JERSEY, bx + (f32)i * 2.1f, 0.02f, bz, jr, 1.0f, 1.2f);
      cwa_box(A, COD_SURFACE_CONCRETE, bx + (f32)i * 2.1f, 0.46f, bz, 0.62f, 0.92f, 1.9f, 0.0f);
    }
    cw_rubble_mound(A, rng, bx - 3.4f, 0.0f, bz, 2.2f, 30, CW_CONCRETE);
    cw_rubble_mound(A, rng, bx + 3.6f, 0.0f, bz, 2.0f, 26, CW_CONCRETE);
    cwa_box(A, COD_SURFACE_CONCRETE, bx, 1.4f, bz + (bz > 0 ? 1.4f : -1.4f), 16.0f, 2.8f, 1.2f, 0.0f);
    for (i32 i = 0; i < 14; i++) {
      f32 px = bx + cod_rng_range(rng, -7.0f, 7.0f);
      f32 pz = bz + cod_rng_range(rng, -2.0f, 2.0f);
      u32 id = CW_PICK(rng, CW_L_PERIM_JUNK);
      f32 ry = cod_rng_float(rng) * CW_TAU32;
      f32 sc = cod_rng_range(rng, 0.7f, 1.3f);
      cwa_put_g(A, id, px, cw_ground_y(px, pz) + 0.03f, pz, ry, sc, 1.4f);
    }
  }
}
