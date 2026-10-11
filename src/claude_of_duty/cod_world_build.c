// Claude of Duty world buildings: footprints, per-side facade programmes, slabs, roofs and interiors.

typedef struct CwFloor {
  f32 x, z, w, d;
} CwFloor;

typedef struct CwWindowInfo {
  i32 side, f;
  f32 x, y, w, h;
  m4 pm;
  u32 state;
} CwWindowInfo;

typedef struct CwBalconyInfo {
  i32 side;
  f32 x, y, w, d;
  m4 pm;
} CwBalconyInfo;

typedef struct CwAwningInfo {
  i32 side;
  f32 x, y, w;
  m4 pm;
} CwAwningInfo;

typedef struct CwDoorInfo {
  i32 side;
  f32 x;
  m4 pm;
  v3 wp;
} CwDoorInfo;

DYN_ARRAY_TYPEDEF(CwWindowInfo)
DYN_ARRAY_TYPEDEF(CwBalconyInfo)
DYN_ARRAY_TYPEDEF(CwAwningInfo)
DYN_ARRAY_TYPEDEF(CwDoorInfo)

typedef struct CwBuildInfo {
  const CwBuildingSpec *spec;
  f32 floor_y[8];
  u32 floor_count;
  f32 roof_y;
  CwFloor roof_spec;
  DynArray(CwWindowInfo) windows;
  DynArray(CwBalconyInfo) balconies;
  DynArray(CwAwningInfo) awnings;
  DynArray(CwDoorInfo) doors;
} CwBuildInfo;

#define CW_GROUND_H 3.45f
#define CW_UPPER_H 3.05f
#define CW_WALL_T 0.34f

hz_internal f32 cw_side_ry(i32 side) { return side == 0 ? 0.0f : side == 1 ? -CW_PI_F / 2 : side == 2 ? CW_PI_F : CW_PI_F / 2; }

// the panel matrix of one side of a rect at height y: +Z points into the building
hz_internal m4 cw_panel_matrix(f32 x, f32 z, f32 w, f32 d, i32 side, f32 y) {
  f32 px = x, pz = z;
  if (side == 0)
    pz = z - d / 2;
  else if (side == 2)
    pz = z + d / 2;
  else if (side == 1)
    px = x + w / 2;
  else
    px = x - w / 2;
  return cw_trs(px, y, pz, cw_side_ry(side), 1, 1, 1, 0, 0);
}

hz_internal f32 cw_side_len(const CwFloor *s, i32 side) { return (side == 0 || side == 2) ? s->w : s->d; }

// per-floor footprint: every floor at or above the setback is pulled back from one face
hz_internal CwFloor cw_floor_spec(const CwBuildingSpec *s, u32 f) {
  CwFloor o = {s->x, s->z, s->w, s->d};
  if (!s->has_setback || (i32)f < s->setback_from)
    return o;
  f32 d = s->setback_depth;
  i32 side = s->setback_side;
  if (side == 1) {
    o.x = s->x - d / 2;
    o.w = s->w - d;
  } else if (side == 3) {
    o.x = s->x + d / 2;
    o.w = s->w - d;
  } else if (side == 0) {
    o.z = s->z + d / 2;
    o.d = s->d - d;
  } else {
    o.z = s->z - d / 2;
    o.d = s->d - d;
  }
  return o;
}

hz_internal u32 cw_trim_key(const CwBuildingSpec *s) { return s->trim_key >= 0 ? (u32)s->trim_key : CW_CONCRETE; }

// repair-render key per wall colour: close in value, different in mix
hz_internal u32 cw_patch_key(u32 wall) {
  switch (wall) {
  case CW_PLASTER_CREAM: return CW_PLASTER_SAND;
  case CW_PLASTER_SAND: return CW_PLASTER_CREAM;
  case CW_PLASTER_BLUE: return CW_CONCRETE;
  case CW_PLASTER_PINK: return CW_PLASTER_SAND;
  case CW_PLASTER_WHITE: return CW_CONCRETE;
  default: return CW_PLASTER_SAND;
  }
}

// the strip of roof left exposed by a setback: slab, coping and a parapet
hz_internal void cw_terrace(CwA *A, const CwBuildingSpec *spec, f32 y) {
  i32 side = spec->setback_side;
  f32 d = spec->setback_depth;
  b32 horiz = side == 1 || side == 3;
  f32 sign = (side == 1 || side == 2) ? 1.0f : -1.0f;
  f32 cx = horiz ? spec->x + sign * (spec->w / 2 - d / 2) : spec->x;
  f32 cz = horiz ? spec->z : spec->z + sign * (spec->d / 2 - d / 2);
  f32 sx = horiz ? d : spec->w;
  f32 sz = horiz ? spec->d : d;
  cwa_add(A, CW_ROOF_SCREED, BOX(A), LL(IDENT, cx, y - 0.13f, cz, 0, sx + 0.08f, 0.26f, sz + 0.08f), CW_M(0.45f, 0.3f, 0.15f));
  cwa_box(A, COD_SURFACE_CONCRETE, cx, y - 0.13f, cz, sx + 0.08f, 0.26f, sz + 0.08f, 0.0f);
  f32 ph = 0.92f;
  f32 px = horiz ? spec->x + sign * (spec->w / 2 - 0.11f) : spec->x;
  f32 pz = horiz ? spec->z : spec->z + sign * (spec->d / 2 - 0.11f);
  cwa_add(A, spec->wall_key, BOX(A), LL(IDENT, px, y + ph / 2, pz, 0, horiz ? 0.22f : spec->w + 0.1f, ph, horiz ? spec->d + 0.1f : 0.22f), CW_M(0.5f, 0.5f, 0.2f));
  cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(IDENT, px, y + ph + 0.05f, pz, 0, horiz ? 0.32f : spec->w + 0.2f, 0.1f, horiz ? spec->d + 0.2f : 0.32f), CW_M(0.8f, 0.35f, 0.1f));
  cwa_box(A, COD_SURFACE_CONCRETE, px, y + ph / 2, pz, horiz ? 0.26f : spec->w + 0.1f, ph + 0.1f, horiz ? spec->d + 0.1f : 0.26f, 0.0f);
  for (i32 s = -1; s <= 1; s += 2) {
    f32 ex = horiz ? cx : spec->x + (f32)s * (spec->w / 2 - 0.11f);
    f32 ez = horiz ? spec->z + (f32)s * (spec->d / 2 - 0.11f) : cz;
    cwa_add(A, spec->wall_key, BOX(A), LL(IDENT, ex, y + ph / 2, ez, 0, horiz ? d : 0.22f, ph, horiz ? 0.22f : d), CW_M(0.5f, 0.5f, 0.2f));
    cwa_box(A, COD_SURFACE_CONCRETE, ex, y + ph / 2, ez, horiz ? d : 0.26f, ph, horiz ? 0.26f : d, 0.0f);
  }
}

// floor slab for one level, with the stairwell void left open
hz_internal void cw_interior_slab(CwA *A, const CwBuildingSpec *spec, const CwFloor *fs, f32 y, f32 t, u32 level, b32 roof) {
  f32 iw = fs->w - t * 2, id = fs->d - t * 2;
  u32 key = roof ? CW_ROOF_SCREED : CW_FLOOR_CONCRETE;
  b32 has_hole = spec->enterable && level < 4 && spec->holes[level].present;
  f32 thick = roof ? 0.26f : 0.2f;
  f32 m0 = roof ? 0.45f : 0.3f, m1 = roof ? 0.25f : 0.55f, m2 = roof ? 0.12f : 0.35f;
  if (!has_hole) {
    cwa_add(A, key, BOX(A), LL(IDENT, fs->x, y - thick / 2, fs->z, 0, iw, thick, id), CW_M(m0, m1, m2));
    cwa_box(A, COD_SURFACE_CONCRETE, fs->x, y - thick / 2, fs->z, iw, thick, id, 0.0f);
  } else {
    const CwStairHole *hole = &spec->holes[level];
    f32 x0 = fs->x - iw / 2, x1 = fs->x + iw / 2, z0 = fs->z - id / 2, z1 = fs->z + id / 2;
    f32 hx0 = hole->x0, hx1 = hole->x1, hz0 = hole->z0, hz1 = hole->z1;
    f32 parts[4][4] = {{x0, z0, x1, hz0}, {x0, hz1, x1, z1}, {x0, hz0, hx0, hz1}, {hx1, hz0, x1, hz1}};
    for (u32 i = 0; i < 4; i++) {
      f32 w = parts[i][2] - parts[i][0], d = parts[i][3] - parts[i][1];
      if (w < 0.05f || d < 0.05f)
        continue;
      f32 cx = (parts[i][0] + parts[i][2]) / 2, cz = (parts[i][1] + parts[i][3]) / 2;
      cwa_add(A, key, BOX(A), LL(IDENT, cx, y - thick / 2, cz, 0, w, thick, d), CW_M(m0, m1, m2));
      cwa_box(A, COD_SURFACE_CONCRETE, cx, y - thick / 2, cz, w, thick, d, 0.0f);
    }
  }
  if (!roof && spec->enterable) {
    i32 n = (i32)m_maxf(2.0f, m_roundf(id / 1.5f));
    for (i32 i = 0; i < n; i++) {
      f32 bz = fs->z - id / 2 + (((f32)i + 0.5f) / (f32)n) * id;
      cwa_add(A, CW_WOOD_DARK, BOX(A), LL(IDENT, fs->x, y - thick - 0.08f, bz, 0, iw, 0.16f, 0.13f), CW_M(0.4f, 0.6f, 0.5f));
    }
  }
}

hz_internal const u16 CW_L_DOOR_LEAF[] = {CW_METAL_GREEN, CW_METAL_BLUE, CW_WOOD_DARK};
hz_internal const u16 CW_L_SHUTTER[] = {CW_METAL_BLUE, CW_METAL_GREEN, CW_WOOD_DARK};

typedef struct CwDeco {
  u32 kind;
  CwOpening o;
  f32 bx, bw, drop, bwid;
  b32 awning;
  u32 state;
  b32 broken;
} CwDeco;

// one side of one floor: bays, wall, openings, runoff, courses, damage
hz_internal void cw_build_facade(CwA *A, CodRng *rng, const CwBuildingSpec *bspec, const CwFloor *fs, CwBuildInfo *info, i32 side, u32 f, f32 y, f32 h, u32 floors) {
  f32 t = CW_WALL_T;
  u32 wall_key = bspec->wall_key;
  i32 street_side = bspec->street_side;
  f32 len = cw_side_len(fs, side);
  m4 pm = cw_panel_matrix(fs->x, fs->z, fs->w, fs->d, side, y);
  b32 street = side == street_side;
  b32 secondary = bspec->secondary_side == side;
  b32 open_face = street || secondary;
  f32 damage = bspec->damage;

  i32 bays = (i32)m_maxf(1.0f, m_roundf(len / 3.05f));
  f32 bw = len / (f32)bays;
  CwOpening *openings = NULL;
  CwDeco *deco = NULL;
  u32 on = 0, dn = 0;
  b32 ruin_top = bspec->ruin && f == floors - 1;

  tctx_temp_allocator_scope(ta, allocator_as_arena(&A->al)) {
    openings = ALLOC_ARRAY(&ta.allocator, CwOpening, (u32)bays + 1);
    deco = ALLOC_ARRAY(&ta.allocator, CwDeco, (u32)bays * 2 + 2);

    for (i32 b = 0; b < bays; b++) {
      f32 bx = -len / 2 + ((f32)b + 0.5f) * bw;
      f32 room = m_minf(bw - 1.0f, 2.6f);
      u32 kind = 0; // blank
      if (f == 0) {
        if (open_face) {
          b32 shop_here = room > 2.0f && cod_rng_f64(rng) < (street ? 0.5 : 0.25);
          if (bspec->door_bay[side] == b)
            kind = CWO_DOOR;
          else if (shop_here)
            kind = CWO_SHOP;
          else if (cod_rng_f64(rng) < 0.72)
            kind = CWO_WINDOW;
        } else if (cod_rng_f64(rng) < 0.4) {
          kind = CWO_WINDOW;
        }
      } else {
        if (cod_rng_f64(rng) < (open_face ? 0.88 : 0.6)) {
          kind = (bspec->arches && f == 1) ? CWO_ARCH : CWO_WINDOW;
          f32 bal = bspec->balconies > 0.0f ? bspec->balconies : 0.35f;
          if (open_face && f >= 1 && cod_rng_f64(rng) < (f64)bal)
            kind = CWO_BALCONY_DOOR;
        }
      }
      if (ruin_top && cod_rng_f64(rng) < 0.5)
        kind = kind == 0 ? 0 : CWO_RAGGED;

      b32 forced = bspec->has_forced && bspec->forced_side == side && bspec->forced_floor == (i32)f && bspec->forced_bay == b;
      if (forced)
        kind = CWO_SHOP;

      switch (kind) {
      case CWO_DOOR: {
        CwOpening o = {.x = bx, .y = 1.08f, .w = 1.12f, .h = 2.16f, .kind = CWO_DOOR};
        openings[on++] = o;
        deco[dn++] = (CwDeco){.kind = CWO_DOOR, .o = o, .bx = bx};
        CwDoorInfo di = {.side = side, .x = bx, .pm = pm, .wp = cw_world_of(pm, bx, 0, 0)};
        dyn_arr_append(info->doors, di);
        break;
      }
      case CWO_SHOP: {
        f32 sw = m_minf(bw - 0.75f, 3.1f);
        CwOpening o = {.x = bx, .y = 1.32f, .w = sw, .h = 2.58f, .kind = CWO_SHOP};
        openings[on++] = o;
        f32 drop;
        if (forced) {
          drop = bspec->forced_drop;
        } else if (cod_rng_f64(rng) < 0.5) {
          drop = cod_rng_range(rng, 0.1f, 0.55f);
        } else {
          drop = 0.0f;
        }
        deco[dn++] = (CwDeco){.kind = CWO_SHOP, .o = o, .bx = bx, .bw = bw, .drop = drop};
        if (cod_rng_f64(rng) < 0.8) {
          f32 aw = sw + 0.5f;
          deco[dn++] = (CwDeco){.kind = 100, .o = o, .bx = bx, .bw = aw};
          CwAwningInfo ai = {.side = side, .x = bx, .y = o.y + o.h / 2 + 0.55f, .w = aw, .pm = pm};
          dyn_arr_append(info->awnings, ai);
        }
        break;
      }
      case CWO_WINDOW: {
        f32 ww = m_minf(room, cod_rng_range(rng, 1.05f, 1.3f));
        f32 wh = f == 0 ? 1.62f : 1.48f;
        CwOpening o = {.x = bx, .y = (f == 0 ? 1.05f : 0.95f) + wh / 2, .w = ww, .h = wh, .kind = CWO_WINDOW};
        openings[on++] = o;
        b32 broken = cod_rng_f64(rng) < (f64)damage * 1.6;
        u32 st = broken ? CWW_OPEN : cw_window_state(rng, f, damage, !open_face || f > 0);
        deco[dn++] = (CwDeco){.kind = CWO_WINDOW, .o = o, .bx = bx, .state = st, .broken = broken};
        CwWindowInfo wi = {.side = side, .f = (i32)f, .x = bx, .y = o.y, .w = ww, .h = wh, .pm = pm, .state = st};
        dyn_arr_append(info->windows, wi);
        break;
      }
      case CWO_ARCH: {
        f32 ww = m_minf(room, 1.35f);
        CwOpening o = {.x = bx, .y = 1.05f + 0.9f, .w = ww, .h = 1.9f, .arch = 0.62f, .kind = CWO_ARCH};
        openings[on++] = o;
        u32 st = cw_window_state(rng, f, damage, true);
        deco[dn++] = (CwDeco){.kind = CWO_ARCH, .o = o, .bx = bx, .state = st};
        CwWindowInfo wi = {.side = side, .f = (i32)f, .x = bx, .y = o.y, .w = ww, .h = o.h, .pm = pm, .state = st};
        dyn_arr_append(info->windows, wi);
        break;
      }
      case CWO_BALCONY_DOOR: {
        f32 ww = m_minf(room, 1.15f);
        CwOpening o = {.x = bx, .y = 1.12f, .w = ww, .h = 2.24f, .kind = CWO_BALCONY_DOOR};
        openings[on++] = o;
        deco[dn++] = (CwDeco){.kind = CWO_BALCONY_DOOR, .o = o, .bx = bx, .bwid = m_minf(bw - 0.35f, 2.6f)};
        break;
      }
      case CWO_RAGGED: {
        CwOpening o = {.x = bx, .y = h * 0.55f, .w = m_minf(bw - 0.4f, 2.2f), .h = h * 0.8f, .ragged = 0.22f, .kind = CWO_RAGGED};
        openings[on++] = o;
        break;
      }
      default: break;
      }
    }

    // the wall itself
    b32 is_top = f == floors - 1;
    CwFacadeSpec wall = {.w = len, .h = h + (is_top ? 0.02f : 0.0f), .t = t, .key = wall_key, .openings = openings, .opening_count = on,
                         .top_ragged = bspec->ruin && is_top && (side == street_side || side == bspec->ruin_side), .ragged_amp = 0.55f,
                         .jag = (is_top && !bspec->ruin) ? 0.03f : 0.0f, .warp = 0.02f, .paint = CW_PAINT_FACADE, .paint_p = f == 0 ? 1.0f : 0.0f};
    cw_facade_wall(A, rng, pm, &wall);

    for (u32 i = 0; i < dn; i++) {
      const CwDeco *dc = &deco[i];
      switch (dc->kind) {
      case CWO_DOOR: {
        f32 open = cod_rng_f64(rng) < 0.45 ? cod_rng_range(rng, 0.5f, 1.6f) : 0.0f;
        u32 leaf = CW_PICK(rng, CW_L_DOOR_LEAF);
        cw_door_unit(A, pm, &dc->o, &(CwDoorOpts){.t = t, .open = open, .leaf_key = leaf});
        break;
      }
      case CWO_SHOP:
        cw_shopfront(A, pm, &dc->o, rng, t, dc->drop);
        break;
      case 100: {
        f32 depth = cod_rng_range(rng, 1.3f, 1.9f);
        u32 key = cw_pick_fabric3(rng);
        b32 legs = cod_rng_f64(rng) < 0.4;
        cw_awning(A, pm, dc->bx, dc->o.y + dc->o.h / 2 + 0.55f, dc->bw, rng, &(CwAwningOpts){.depth = depth, .key = key, .legs = legs});
        break;
      }
      case CWO_WINDOW: {
        b32 grille = f == 0 && dc->state != CWW_BOARDED && cod_rng_f64(rng) < 0.55;
        b32 shutters = f > 0 && (dc->state == CWW_SHUTTERED || cod_rng_f64(rng) < 0.4);
        u32 shutter_key = CW_PICK(rng, CW_L_SHUTTER);
        b32 curtain = dc->state == CWW_CURTAIN || (dc->state == CWW_GLAZED && cod_rng_f64(rng) < 0.25);
        CwWindowOpts wo = {.t = t, .broken = dc->broken, .state = dc->state, .back = !bspec->enterable, .grille = grille, .shutters = shutters,
                           .shutter_key = shutter_key, .curtain = curtain};
        cw_window_unit(A, pm, &dc->o, rng, &wo);
        break;
      }
      case CWO_ARCH: {
        b32 broken = cod_rng_f64(rng) < 0.2;
        b32 curtain = dc->state == CWW_CURTAIN || cod_rng_f64(rng) < 0.3;
        CwWindowOpts wo = {.t = t, .broken = broken, .state = dc->state, .back = !bspec->enterable, .shutters = false, .curtain = curtain, .no_lintel = true};
        cw_window_unit(A, pm, &dc->o, rng, &wo);
        break;
      }
      case CWO_BALCONY_DOOR: {
        f32 open = cod_rng_f64(rng) < 0.5 ? cod_rng_range(rng, 0.6f, 1.5f) : 0.0f;
        cw_door_unit(A, pm, &dc->o, &(CwDoorOpts){.t = t, .open = open, .leaf_key = CW_WOOD_DARK});
        f32 bdepth = cod_rng_range(rng, 1.0f, 1.35f);
        b32 concrete = cod_rng_f64(rng) < 0.45;
        cw_balcony(A, pm, dc->bx, 0.02f, dc->bwid, &(CwBalconyOpts){.depth = bdepth, .concrete_rail = concrete, .key = wall_key});
        CwBalconyInfo bi = {.side = side, .x = dc->bx, .y = 0.02f, .w = dc->bwid, .d = bdepth, .pm = pm};
        dyn_arr_append(info->balconies, bi);
        break;
      }
      default: break;
      }
    }

    // rain runoff below every opening and ledge, from a stream keyed to this panel
    f64 key64 = m_floor((f64)(fs->x + 512.0f) * 977.0 + (f64)(fs->z + 512.0f) * 7919.0 + 0.5);
    u32 wseed = (u32)((i32)key64 ^ (side * 131 + (i32)f * 1237));
    CodRng wr = cod_rng_make(wseed);
    for (u32 i = 0; i < on; i++) {
      const CwOpening *o = &openings[i];
      if (o->kind == CWO_RAGGED)
        continue;
      f32 sill_y = o->y - o->h / 2;
      if (cod_rng_f64(&wr) < 0.22)
        continue;
      f32 run = m_minf(cod_rng_range(&wr, 0.7f, 1.8f), m_maxf(0.25f, sill_y - 0.12f));
      f32 rw = o->w * cod_rng_range(&wr, 0.6f, 1.0f);
      f32 amount = cod_rng_range(&wr, 0.72f, 1.0f);
      CwGeoId g = cwa_runoff(A, &wr, rw, run, amount, 5);
      f32 jx = cod_rng_range(&wr, -0.1f, 0.1f);
      cwa_add(A, wall_key, g, LL(pm, o->x + jx, sill_y - 0.03f, -0.012f, 0, 1, 1, 1), NULL);
      if (cod_rng_f64(&wr) < 0.55) {
        f32 sgn = cod_rng_f64(&wr) < 0.5 ? -1.0f : 1.0f;
        f32 run2 = m_minf(cod_rng_range(&wr, 0.5f, 1.3f), m_maxf(0.2f, sill_y - 0.1f));
        f32 rw2 = cod_rng_range(&wr, 0.1f, 0.22f);
        f32 amount2 = cod_rng_range(&wr, 0.8f, 1.0f);
        CwGeoId g2 = cwa_runoff(A, &wr, rw2, run2, amount2, 3);
        f32 ox = sgn * o->w * cod_rng_range(&wr, 0.32f, 0.5f);
        cwa_add(A, wall_key, g2, LL(pm, o->x + ox, sill_y - 0.02f, -0.013f, 0, 1, 1, 1), NULL);
      }
    }
    if (open_face && cod_rng_f64(&wr) < 0.8) {
      f32 rw = cod_rng_range(&wr, 0.18f, 0.4f);
      f32 rl = cod_rng_range(&wr, 1.0f, 1.8f);
      f32 amount = cod_rng_range(&wr, 0.78f, 1.0f);
      CwGeoId g = cwa_runoff(A, &wr, rw, rl, amount, 4);
      f32 px = cod_rng_range(&wr, -len / 2 + 0.4f, len / 2 - 0.4f);
      cwa_add(A, wall_key, g, LL(pm, px, h - 0.16f, -0.012f, 0, 1, 1, 1), NULL);
    }
  }

  // string course between floors
  if (f < floors - 1 && (open_face || cod_rng_f64(rng) < 0.5))
    cwa_add(A, cw_trim_key(bspec), BOX_SOFT(A), LL(pm, 0, h - 0.09f, -0.055f, 0, len + 0.06f, 0.13f, 0.12f), CW_M(0.7f, 0.45f, 0.2f));
  // top cornice
  if (f == floors - 1 && !bspec->ruin)
    cwa_add(A, cw_trim_key(bspec), BOX_SOFT(A), LL(pm, 0, h - 0.14f, -0.11f, 0, len + 0.14f, 0.22f, 0.2f), CW_M(0.75f, 0.5f, 0.25f));

  // damage: spalled render exposing brick, bullet-pocked plaster
  f32 dmg = damage;
  i32 spalls = (i32)m_roundf(dmg * 5.0f * (open_face ? 1.4f : 0.7f));
  for (i32 i = 0; i < spalls; i++) {
    f32 sx = cod_rng_range(rng, -len / 2 + 0.5f, len / 2 - 0.5f);
    f32 sy = cod_rng_range(rng, 0.4f, h - 0.5f);
    f32 sw = cod_rng_range(rng, 0.35f, 1.0f);
    f32 sh = cod_rng_range(rng, 0.3f, 0.8f);
    CwGeoId g = cwa_spall(A, sw, sh, 0.03f);
    cwa_add(A, CW_BRICK_FINE, g, LL(pm, sx, sy, 0.01f, 0, 1, 1, 1), NULL);
  }
  if (open_face && cod_rng_f64(rng) < 0.5) {
    f32 px = cod_rng_range(rng, -len / 2 + 1.0f, len / 2 - 1.0f);
    f32 py = cod_rng_range(rng, 0.5f, h - 1.2f);
    f32 sw = cod_rng_range(rng, 0.6f, 1.4f);
    f32 sh = cod_rng_range(rng, 0.5f, 1.1f);
    CwGeoId g = cwa_spall(A, sw, sh, 0.02f);
    cwa_add(A, cw_patch_key(wall_key), g, LL(pm, px, py, 0.013f, 0, 1, 1, 1), NULL);
  }

  // bullet pocks, clustered where somebody took cover
  if (cwa_has(A, CWP_POCK)) {
    i32 bursts = (i32)m_roundf(dmg * 6.0f) + (open_face ? 2 : 0);
    for (i32 i = 0; i < bursts; i++) {
      f32 cx = cod_rng_range(rng, -len / 2 + 0.4f, len / 2 - 0.4f);
      f32 cy = cod_rng_range(rng, 0.5f, m_minf(h - 0.4f, 3.0f));
      i32 n = cod_rng_int(rng, 3, 9);
      for (i32 j = 0; j < n; j++) {
        f32 px = cx + cod_rng_gauss(rng) * 0.45f;
        f32 py = cy + cod_rng_gauss(rng) * 0.32f;
        if (m_fabsf(px) > len / 2 - 0.15f)
          continue;
        if (py < 0.15f || py > h - 0.15f)
          continue;
        b32 in_hole = false;
        for (u32 k = 0; k < on; k++) {
          const CwOpening *o = &openings[k];
          if (px > o->x - o->w / 2 - 0.05f && px < o->x + o->w / 2 + 0.05f && py > o->y - o->h / 2 - 0.05f && py < o->y + o->h / 2 + 0.05f) {
            in_hole = true;
            break;
          }
        }
        if (in_hole)
          continue;
        v3 wp = cw_world_of(pm, px, py, 0.0015f);
        f32 s = cod_rng_range(rng, 0.55f, 1.5f);
        f32 sz = cod_rng_range(rng, 0.5f, 1.2f);
        f32 g = cod_rng_range(rng, 0.7f, 1.3f);
        cwa_put_s_m(A, CWP_POCK, wp.x, wp.y, wp.z, cw_side_ry(side) + CW_PI_F, s, s, sz, 1.0f, g, 1.0f, 0.0f, 0.0f);
      }
    }
  }
}

// ---------------------------------------------------------------- interior --

hz_internal void cw_build_interior(CwA *A, CodRng *rng, const CwBuildingSpec *spec, CwBuildInfo *info, f32 t, u32 floors) {
  f32 it = 0.16f;
  CwFloor g0 = cw_floor_spec(spec, 0);
  cwa_add(A, CW_FLOOR_CONCRETE, BOX(A), LL(IDENT, g0.x, 0.06f, g0.z, 0, g0.w - t * 2, 0.14f, g0.d - t * 2), CW_M(0.3f, 0.6f, 0.4f));
  cwa_box(A, COD_SURFACE_CONCRETE, g0.x, 0.06f, g0.z, g0.w - t * 2, 0.16f, g0.d - t * 2, 0.0f);

  for (u32 f = 0; f < floors; f++) {
    CwFloor fs = cw_floor_spec(spec, f);
    f32 iw = fs.w - t * 2, id = fs.d - t * 2;
    f32 x0 = fs.x - iw / 2, z0 = fs.z - id / 2;
    f32 fy = info->floor_y[f] + (f == 0 ? 0.13f : 0.0f);
    f32 fh = f == 0 ? CW_GROUND_H - 0.13f : CW_UPPER_H;
    const CwRoomPlan *plan = spec->room_count ? &spec->rooms[f < spec->room_count ? f : spec->room_count - 1] : NULL;
    if (plan) {
      for (u32 wi = 0; wi < plan->wall_count; wi++) {
        const CwRoomWall *wl = &plan->walls[wi];
        f32 wx0 = x0 + wl->ax * iw, wz0 = z0 + wl->az * id, wx1 = x0 + wl->bx * iw, wz1 = z0 + wl->bz * id;
        f32 len = m_sqrtf((wx1 - wx0) * (wx1 - wx0) + (wz1 - wz0) * (wz1 - wz0));
        f32 ry = m_atan2f(wx1 - wx0, wz1 - wz0) - CW_PI_F / 2;
        m4 pm = cw_trs((wx0 + wx1) / 2 - m_sinf(ry) * (it / 2), fy, (wz0 + wz1) / 2 - m_cosf(ry) * (it / 2), ry, 1, 1, 1, 0, 0);
        CwOpening hole = {.x = -len / 2 + wl->door * len, .y = 1.06f, .w = 1.05f, .h = 2.12f};
        CwFacadeSpec wall = {.w = len, .h = fh, .t = it, .key = CW_PLASTER_WHITE, .openings = &hole, .opening_count = 1, .warp = 0.012f,
                             .bevel = 0.012f, .paint = CW_PAINT_PARTITION};
        cw_facade_wall(A, rng, pm, &wall);
        b32 leaf = cod_rng_f64(rng) < 0.4;
        cw_door_unit(A, pm, &hole, &(CwDoorOpts){.t = it, .no_leaf = !leaf, .open = 1.4f, .leaf_key = CW_WOOD_DARK});
      }
    }

    // stairs rising out of this floor
    for (u32 si = 0; si < spec->flight_count; si++) {
      const CwStairFlight *fl = &spec->flights[si];
      if (fl->floor != (i32)f)
        continue;
      f32 base = info->floor_y[f] + (f == 0 ? 0.13f : 0.0f);
      f32 next_y = (f + 1 < info->floor_count) ? info->floor_y[f + 1] : info->roof_y;
      f32 climb = next_y - base;
      i32 steps = (i32)m_maxf(6.0f, m_roundf(climb / 0.19f));
      f32 rise = climb / (f32)steps;
      f32 run = 0.275f;
      f32 sw = fl->w;
      m4 pm = cw_trs(x0 + fl->x * iw, base, z0 + fl->z * id, fl->ry, 1, 1, 1, 0, 0);
      cw_stair_run(A, pm, 0, 0, 0, sw, (u32)steps, rise, run, &(CwStairOpts){.key = CW_CONCRETE_DARK, .railing = fl->railing_right ? 1 : 1});
      f32 D = (f32)steps * run, H = (f32)steps * rise;
      cwa_add(A, CW_CONCRETE_DARK, BOX(A), LL(pm, 0, H - 0.1f, D + 0.55f, 0, sw + 0.1f, 0.2f, 1.1f), CW_M(0.4f, 0.5f, 0.3f));
      v3 wp = cw_world_of(pm, 0, H - 0.1f, D + 0.55f);
      cwa_box(A, COD_SURFACE_CONCRETE, wp.x, wp.y, wp.z, sw + 0.1f, 0.2f, 1.1f, fl->ry);
    }

    // furnishing
    if (plan) {
      for (u32 ri = 0; ri < plan->furnish_count; ri++) {
        const CwFurnish *r = &plan->furnish[ri];
        CwRoom room = {.kind = r->kind, .street = spec->street_side, .x0 = x0 + r->x0 * iw, .z0 = z0 + r->z0 * id,
                       .x1 = x0 + r->x1 * iw, .z1 = z0 + r->z1 * id, .y = fy, .h = fh};
        cw_furnish_room(A, rng, &room);
      }
    }
  }

  // roof access: a stair penthouse box with an open doorway
  if (spec->roof_access) {
    CwFloor rs = cw_floor_spec(spec, floors - 1);
    f32 riw = rs.w - t * 2, rid = rs.d - t * 2;
    const CwStairFlight *st = &spec->flights[spec->flight_count - 1];
    f32 px = rs.x - riw / 2 + st->x * riw;
    f32 pz = rs.z - rid / 2 + st->z * rid + 3.6f;
    f32 y = info->roof_y;
    for (i32 side = 0; side < 4; side++) {
      m4 pm = cw_panel_matrix(px, pz, 2.4f, 2.6f, side, y);
      CwOpening door = {.x = 0, .y = 1.08f, .w = 1.05f, .h = 2.16f};
      CwFacadeSpec wall = {.w = (side == 0 || side == 2) ? 2.4f : 2.6f, .h = 2.5f, .t = 0.22f, .key = spec->wall_key, .openings = &door,
                           .opening_count = side == 2 ? 1u : 0u, .warp = 0.015f};
      cw_facade_wall(A, rng, pm, &wall);
    }
    cwa_add(A, CW_CONCRETE, BOX(A), LL(IDENT, px, y + 2.6f, pz, 0, 2.7f, 0.2f, 2.9f), CW_M(0.5f, 0.45f, 0.2f));
    cwa_box(A, COD_SURFACE_CONCRETE, px, y + 2.6f, pz, 2.7f, 0.2f, 2.9f, 0.0f);
  }
}

// ---------------------------------------------------------------- building --

hz_internal CwBuildInfo *cw_build_building(CwA *A, CodRng *rng, const CwBuildingSpec *spec) {
  CwBuildInfo *info = ALLOC(&A->al, CwBuildInfo);
  info->spec = spec;
  info->windows = dyn_arr_new_alloc(&A->al, CwWindowInfo, 16);
  info->balconies = dyn_arr_new_alloc(&A->al, CwBalconyInfo, 8);
  info->awnings = dyn_arr_new_alloc(&A->al, CwAwningInfo, 8);
  info->doors = dyn_arr_new_alloc(&A->al, CwDoorInfo, 8);
  f32 t = CW_WALL_T;
  u32 floors = spec->floors;
  u32 wall_key = spec->wall_key;

  // a base course everywhere: catches the ground grime band
  f32 plinth_h = 0.42f;
  cwa_add(A, CW_CONCRETE, BOX(A), LL(IDENT, spec->x, plinth_h / 2, spec->z, 0, spec->w + 0.14f, plinth_h, spec->d + 0.14f), CW_M(0.55f, 0.75f, 0.45f));
  cwa_box(A, COD_SURFACE_CONCRETE, spec->x, plinth_h / 2, spec->z, spec->w + 0.14f, plinth_h, spec->d + 0.14f, 0.0f);

  f32 y = 0.0f;
  info->floor_count = floors;
  for (u32 f = 0; f < floors; f++) {
    f32 h = f == 0 ? CW_GROUND_H : CW_UPPER_H;
    CwFloor fs = cw_floor_spec(spec, f);
    info->floor_y[f] = y;
    for (i32 side = 0; side < 4; side++) {
      if (spec->skip_sides & (1u << side))
        continue;
      cw_build_facade(A, rng, spec, &fs, info, side, f, y, h, floors);
    }
    y += h;
    if (f < floors - 1) {
      CwFloor nfs = cw_floor_spec(spec, f + 1);
      cw_interior_slab(A, spec, &nfs, y, t, f + 1, false);
      if (spec->has_setback && (i32)(f + 1) == spec->setback_from)
        cw_terrace(A, spec, y);
    }
  }
  info->roof_y = y;

  // roof
  CwFloor ts = cw_floor_spec(spec, floors - 1);
  cw_interior_slab(A, spec, &ts, y, t, floors, true);
  cw_parapet(A, wall_key, ts.x, ts.z, ts.w + 0.1f, ts.d + 0.1f, y, rng, 0.78f, 0.22f);
  info->roof_spec = ts;

  if (spec->enterable) {
    cw_build_interior(A, rng, spec, info, t, floors);
  } else {
    // a dark core so windows read as depth, sized off the smallest floor plate
    CwFloor top = cw_floor_spec(spec, floors - 1);
    f32 inset = 2.0f;
    f32 cw_ = m_maxf(1.0f, top.w - inset * 2);
    f32 cd = m_maxf(1.0f, top.d - inset * 2);
    f32 core_h = m_maxf(0.5f, y - 0.45f);
    cwa_add(A, CW_INTERIOR_SHELL, BOX(A), LL(IDENT, top.x, core_h / 2, top.z, 0, cw_, core_h, cd), CW_M(0.1f, 0.95f, 0.9f));
    cwa_box(A, COD_SURFACE_CONCRETE, top.x, core_h / 2, top.z, cw_, core_h, cd, 0.0f);
    for (u32 f = 0; f <= floors; f++) {
      CwFloor fs = cw_floor_spec(spec, f < floors - 1 ? f : floors - 1);
      f32 fy = f == 0 ? 0.1f : (f < floors ? info->floor_y[f] : y);
      cwa_add(A, CW_FLOOR_CONCRETE, BOX(A), LL(IDENT, fs.x, fy - 0.06f, fs.z, 0, fs.w - t * 2, 0.16f, fs.d - t * 2), CW_M(0.2f, 0.8f, 0.6f));
      if (f == 0)
        cwa_box(A, COD_SURFACE_CONCRETE, fs.x, fy - 0.06f, fs.z, fs.w, 0.2f, fs.d, 0.0f);
    }
  }

  // a downpipe has to die into the wall it is clipped to
  i32 dp_side = spec->street_side;
  CwFloor whole = {spec->x, spec->z, spec->w, spec->d};
  m4 pmd = cw_panel_matrix(spec->x, spec->z, spec->w, spec->d, dp_side, 0.0f);
  f32 len = cw_side_len(&whole, dp_side);
  i32 sb_side = spec->has_setback ? spec->setback_side : -1;
  f32 dp_top = sb_side == dp_side ? info->floor_y[spec->setback_from] + 0.55f : info->roof_y + 0.4f;
  f32 dx0 = cod_rng_range(rng, -len / 2 + 0.4f, -len / 2 + 1.0f);
  cw_drainpipe(A, pmd, dx0, dp_top, dp_top);
  if (cod_rng_f64(rng) < 0.6) {
    f32 dx1 = cod_rng_range(rng, len / 2 - 1.0f, len / 2 - 0.4f);
    cw_drainpipe(A, pmd, dx1, dp_top, dp_top);
  }
  return info;
}

// a hole in the roof slab and a matching heap of rubble on the floor below
hz_internal void cw_collapse_roof(CwA *A, CodRng *rng, const CwBuildInfo *info, f32 hx, f32 hz) {
  cw_rubble_mound(A, rng, hx, info->floor_y[info->floor_count - 1] + 0.15f, hz, 2.1f, 26, CW_CONCRETE);
}
