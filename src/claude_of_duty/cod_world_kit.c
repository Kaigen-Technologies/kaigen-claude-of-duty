// Claude of Duty world kit: facades, windows, doors, shopfronts, balconies, stairs, awnings, pipes and damage.

#define IDENT m4_identity()
#define LL(pm, x, y, z, ry, sx, sy, sz) cw_ll((pm), (x), (y), (z), (ry), (sx), (sy), (sz), 0.0f, 0.0f)
#define LLR(pm, x, y, z, ry, sx, sy, sz, rx, rz) cw_ll((pm), (x), (y), (z), (ry), (sx), (sy), (sz), (rx), (rz))
#define LLP(pm, x, y, z) cw_ll((pm), (x), (y), (z), 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f)

// rng.pick over the three canopy cloths
hz_internal u32 cw_pick_fabric3(CodRng *rng) {
  u32 i = cod_rng_pick(rng, 3);
  return i == 0 ? CW_FABRIC_RED : i == 1 ? CW_FABRIC_TEAL : CW_FABRIC_CREAM;
}

hz_internal b32 cw_chance(CodRng *rng, f64 p) { return cod_rng_f64(rng) < p; }

// cache kinds
enum { CWK_SASH = 1, CWK_SHUTTER, CWK_DOORLEAF, CWK_ROLLER, CWK_PIPE };

// ---- composites built from the shared boxes (scaled parts merged) ----

typedef struct CwBuilder {
  Allocator *al;
  CwGeo g;
} CwBuilder;

hz_internal void cw_part_plain(CwBuilder *b, f32 sx, f32 sy, f32 sz, f32 x, f32 y, f32 z, f32 rx) {
  tctx_temp_allocator_scope(ta, allocator_as_arena(b->al)) {
    CwGeo p = cw_plain_box(&ta.allocator);
    cw_geo_apply(&p, cw_trs(x, y, z, 0.0f, sx, sy, sz, rx, 0.0f));
    cw_geo_append(&b->g, &p);
  }
}

hz_internal void cw_part_chamfer(CwBuilder *b, f32 sx, f32 sy, f32 sz, f32 bevel, f32 x, f32 y, f32 z) {
  tctx_temp_allocator_scope(ta, allocator_as_arena(b->al)) {
    CwGeo p = cw_chamfer_box(&ta.allocator, sx, sy, sz, bevel);
    cw_geo_translate(&p, x, y, z);
    cw_geo_append(&b->g, &p);
  }
}

// a glazed casement leaf hinged at its left edge
hz_internal CwGeo cw_sash_leaf(Allocator *al, f32 w, f32 h) {
  CwBuilder b = {.al = al, .g = cw_geo_new(al, 24 * 6, 36 * 6)};
  cw_part_plain(&b, 0.05f, h, 0.032f, 0.025f, 0.0f, 0.0f, 0.0f);
  cw_part_plain(&b, 0.05f, h, 0.032f, w - 0.025f, 0.0f, 0.0f, 0.0f);
  cw_part_plain(&b, w, 0.05f, 0.032f, w / 2, h / 2 - 0.025f, 0.0f, 0.0f);
  cw_part_plain(&b, w, 0.05f, 0.032f, w / 2, -h / 2 + 0.025f, 0.0f, 0.0f);
  cw_part_plain(&b, w - 0.08f, 0.038f, 0.026f, w / 2, h * 0.14f, 0.0f, 0.0f);
  cw_part_plain(&b, w - 0.09f, h - 0.09f, 0.008f, w / 2, 0.0f, -0.006f, 0.0f);
  return b.g;
}

// a louvred shutter leaf: stiles, rails and slats; origin at the leaf centre
hz_internal CwGeo cw_shutter_leaf(Allocator *al, f32 w, f32 h) {
  u32 slats = (u32)m_maxf(4.0f, m_floorf((h - 0.16f) / 0.115f));
  CwBuilder b = {.al = al, .g = cw_geo_new(al, 24 * (5 + slats), 36 * (5 + slats))};
  cw_part_plain(&b, 0.05f, h, 0.035f, -w / 2 + 0.025f, 0.0f, 0.0f, 0.0f);
  cw_part_plain(&b, 0.05f, h, 0.035f, w / 2 - 0.025f, 0.0f, 0.0f, 0.0f);
  cw_part_plain(&b, w, 0.05f, 0.035f, 0.0f, h / 2 - 0.025f, 0.0f, 0.0f);
  cw_part_plain(&b, w, 0.05f, 0.035f, 0.0f, -h / 2 + 0.025f, 0.0f, 0.0f);
  cw_part_plain(&b, w, 0.05f, 0.035f, 0.0f, 0.0f, 0.0f, 0.0f);
  for (u32 i = 0; i < slats; i++) {
    f32 y = -h / 2 + 0.09f + ((f32)i / (f32)(slats - 1)) * (h - 0.18f);
    if (m_fabsf(y) < 0.04f)
      continue;
    cw_part_plain(&b, w - 0.08f, 0.06f, 0.014f, 0.0f, y, 0.006f, -0.5f);
  }
  return b.g;
}

hz_internal CwGeo cw_door_leaf(Allocator *al, f32 w, f32 h) {
  CwBuilder b = {.al = al, .g = cw_geo_new(al, 96 * 7, 132 * 7)};
  cw_part_chamfer(&b, w, h, 0.05f, 0.006f, 0, 0, 0);
  cw_part_chamfer(&b, w, 0.1f, 0.062f, 0.006f, 0, h / 2 - 0.07f, 0);
  cw_part_chamfer(&b, w, 0.1f, 0.062f, 0.006f, 0, -h / 2 + 0.07f, 0);
  cw_part_chamfer(&b, w, 0.12f, 0.062f, 0.006f, 0, h * 0.06f, 0);
  cw_part_chamfer(&b, 0.09f, h, 0.062f, 0.006f, -w / 2 + 0.05f, 0, 0);
  cw_part_chamfer(&b, 0.09f, h, 0.062f, 0.006f, w / 2 - 0.05f, 0, 0);
  cw_part_chamfer(&b, 0.035f, 0.13f, 0.035f, 0.006f, w / 2 - 0.16f, -0.02f, 0.05f);
  return b.g;
}

// a corrugated roller shutter, 1 m tall, scaled by the caller; both faces
hz_internal CwGeo cw_roller_shutter(Allocator *al, f32 w, f32 h) {
  CwGeo front;
  ArenaAllocator *conflict = allocator_as_arena(al);
  tctx_temp_allocator_scope(ta, conflict) {
    PgMesh m;
    pg_mesh_begin(&m, ta.temp.arena, 64, 256);
    pg_plane(&m, w, h, 2, 14);
    front = cw_geo_from_pg(al, &m);
  }
  for (u32 i = 0; i < front.nv; i++)
    front.pos[i].z = m_sinf(front.pos[i].y * 90.0f) * 0.008f;
  cw_geo_normals(&front);
  CwGeo out = cw_geo_new(al, front.nv * 2, front.ni * 2);
  cw_geo_append(&out, &front);
  CwGeo back = front;
  back.pos = ALLOC_ARRAY_NO_ZERO(al, v3, front.nv);
  back.nrm = ALLOC_ARRAY_NO_ZERO(al, v3, front.nv);
  mem_cpy(back.pos, front.pos, sizeof(v3) * front.nv);
  mem_cpy(back.nrm, front.nrm, sizeof(v3) * front.nv);
  cw_geo_apply(&back, cw_trs(0, 0, 0, CW_PI_F, 1, 1, 1, 0, 0));
  cw_geo_append(&out, &back);
  return out;
}

// ---- panels ----

typedef struct CwOpening {
  f32 x, y, w, h;
  f32 arch;
  f32 ragged;
  u32 kind;
} CwOpening;

enum { CWO_DOOR = 1, CWO_SHOP, CWO_WINDOW, CWO_ARCH, CWO_BALCONY_DOOR, CWO_RAGGED, CWO_PLAIN };

typedef struct CwFacadeSpec {
  f32 w, h, t;
  u32 key;
  const CwOpening *openings;
  u32 opening_count;
  b32 top_ragged;
  f32 ragged_amp;
  f32 jag;
  f32 warp;
  f32 bevel;
  u32 paint;
  f32 paint_p;
} CwFacadeSpec;

// the solid rectangles left once the holes are cut, for the collision hull
hz_internal void cw_solid_slabs(CwA *A, u32 surface, m4 pm, f32 w, f32 h, f32 t, const CwOpening *holes, u32 hn) {
  tctx_temp_allocator_scope(ta, allocator_as_arena(&A->al)) {
    f32 *xs = ALLOC_ARRAY_NO_ZERO(&ta.allocator, f32, 2 + hn * 2);
    u32 xn = 0;
    xs[xn++] = -w / 2;
    xs[xn++] = w / 2;
    for (u32 i = 0; i < hn; i++) {
      xs[xn++] = m_maxf(-w / 2, holes[i].x - holes[i].w / 2);
      xs[xn++] = m_minf(w / 2, holes[i].x + holes[i].w / 2);
    }
    for (u32 i = 1; i < xn; i++) {
      f32 v = xs[i];
      u32 j = i;
      while (j > 0 && xs[j - 1] > v) {
        xs[j] = xs[j - 1];
        j--;
      }
      xs[j] = v;
    }
    // a Set drops equal values
    u32 un = 0;
    for (u32 i = 0; i < xn; i++)
      if (un == 0 || xs[un - 1] != xs[i])
        xs[un++] = xs[i];
    f32 *sp = ALLOC_ARRAY_NO_ZERO(&ta.allocator, f32, hn * 2 + 2);
    for (u32 i = 0; i + 1 < un; i++) {
      f32 bx0 = xs[i], bx1 = xs[i + 1];
      if (bx1 - bx0 < 1e-4f)
        continue;
      f32 mid = (bx0 + bx1) / 2;
      u32 sn = 0;
      for (u32 k = 0; k < hn; k++) {
        if (mid > holes[k].x - holes[k].w / 2 && mid < holes[k].x + holes[k].w / 2) {
          sp[sn * 2] = m_maxf(0.0f, holes[k].y - holes[k].h / 2);
          sp[sn * 2 + 1] = m_minf(h, holes[k].y + holes[k].h / 2);
          sn++;
        }
      }
      for (u32 a = 1; a < sn; a++) {
        f32 s0 = sp[a * 2], s1 = sp[a * 2 + 1];
        u32 b = a;
        while (b > 0 && sp[(b - 1) * 2] > s0) {
          sp[b * 2] = sp[(b - 1) * 2];
          sp[b * 2 + 1] = sp[(b - 1) * 2 + 1];
          b--;
        }
        sp[b * 2] = s0;
        sp[b * 2 + 1] = s1;
      }
      f32 y = 0.0f;
      for (u32 k = 0; k < sn; k++) {
        f32 s0 = sp[k * 2], s1 = sp[k * 2 + 1];
        if (s0 > y)
          cwa_slab_box(A, surface, pm, (bx0 + bx1) / 2, (y + s0) / 2, bx1 - bx0, s0 - y, t);
        y = m_maxf(y, s1);
      }
      if (y < h)
        cwa_slab_box(A, surface, pm, (bx0 + bx1) / 2, (y + h) / 2, bx1 - bx0, h - y, t);
    }
  }
}

// a wall panel with real openings merged into the key's batch, and its collision slabs
hz_internal void cw_facade_wall(CwA *A, CodRng *rng, m4 pm, const CwFacadeSpec *f) {
  CwSpec sp = {.kind = CWS_PANEL};
  CwPanelSpec *p = &sp.panel;
  p->w = f->w;
  p->h = f->h;
  p->t = f->t;
  p->bevel = f->bevel > 0.0f ? f->bevel : 0.022f;
  p->curve_segments = 7;
  p->top_ragged = f->top_ragged;
  p->ragged_amp = f->ragged_amp > 0.0f ? f->ragged_amp : 0.5f;
  p->jag = f->jag;
  p->warp = f->warp > 0.0f ? f->warp : 0.018f;
  if (f->top_ragged) {
    u32 steps = cw_ragged_steps(f->w);
    f32 *tj = ALLOC_ARRAY(&A->al, f32, steps + 1);
    for (u32 i = 0; rng && i <= steps; i++)
      tj[i] = cod_rng_range(rng, -0.12f, 0.12f);
    p->top_jitter = tj;
  }
  CwHole *holes = ALLOC_ARRAY_NO_ZERO(&A->al, CwHole, f->opening_count + 1);
  for (u32 i = 0; i < f->opening_count; i++) {
    const CwOpening *o = &f->openings[i];
    holes[i] = (CwHole){.x = o->x, .y = o->y, .w = o->w, .h = o->h, .arch = o->arch, .ragged = o->ragged};
    if (o->ragged > 0.0f) {
      f32 *j = ALLOC_ARRAY(&A->al, f32, 18);
      for (u32 k = 0; rng && k < 18; k++)
        j[k] = cod_rng_range(rng, -o->ragged, o->ragged);
      holes[i].jitter = j;
    }
  }
  p->holes = holes;
  p->hole_count = f->opening_count;
  CwGeoId g = cwa_spec(A, sp);
  CwOpts o = {.paint = f->paint, .paint_p = f->paint_p};
  cwa_add(A, f->key, g, pm, &o);
  cw_solid_slabs(A, cw_surface_of(f->key), pm, f->w, f->h, f->t, f->openings, f->opening_count);
}

// ---- windows ----

enum { CWW_BOARDED = 1, CWW_OPEN, CWW_SHUTTERED, CWW_AJAR, CWW_CURTAIN, CWW_LIT, CWW_GLAZED };

hz_internal u32 cw_window_state(CodRng *rng, u32 floor, f32 damage, b32 allow_lit) {
  f64 r = cod_rng_f64(rng);
  b32 upper = floor > 0;
  if (r < 0.07 + (f64)damage * 0.25)
    return CWW_BOARDED;
  if (r < 0.2 + (f64)damage * 0.5)
    return CWW_OPEN;
  if (upper && r < 0.42)
    return CWW_SHUTTERED;
  if (upper && r < 0.52)
    return CWW_AJAR;
  if (r < 0.6)
    return CWW_CURTAIN;
  if (allow_lit && r < 0.66)
    return CWW_LIT;
  return CWW_GLAZED;
}

typedef struct CwWindowOpts {
  f32 t;
  u32 frame_key;
  u32 state;
  b32 broken;
  b32 back;
  b32 no_sill;
  b32 no_lintel;
  b32 grille;
  b32 shutters;
  u32 shutter_key;
  b32 curtain;
  u32 curtain_key;
} CwWindowOpts;

hz_internal CwGeoId cw_cached_sash(CwA *A, f32 sw, f32 h) {
  CwGeoId id;
  if (cwa_cache_find(A, CWK_SASH, cw_q2(sw), cw_q2(h), &id))
    return id;
  return cwa_cache_store(A, CWK_SASH, cw_q2(sw), cw_q2(h), cwa_ready(A, cw_sash_leaf(&A->al, sw, h - 0.06f)));
}

hz_internal CwGeoId cw_cached_shutter(CwA *A, f32 sw, f32 h) {
  CwGeoId id;
  if (cwa_cache_find(A, CWK_SHUTTER, cw_q2(sw), cw_q2(h), &id))
    return id;
  return cwa_cache_store(A, CWK_SHUTTER, cw_q2(sw), cw_q2(h), cwa_ready(A, cw_shutter_leaf(&A->al, sw, h - 0.03f)));
}

// recessed frame, glass or a blown-out hole, sill, lintel, grille, shutters, curtain and a dark room behind
hz_internal void cw_window_unit(CwA *A, m4 pm, const CwOpening *o, CodRng *rng, const CwWindowOpts *op) {
  f32 t = op->t;
  u32 frame_key = op->frame_key ? op->frame_key : CW_WOOD_DARK;
  f32 depth = t * 0.62f;
  f32 w = o->w, h = o->h, x = o->x, y = o->y;
  f32 fw = 0.055f, fd = 0.075f;
  u32 state = op->state;
  b32 broken = op->broken;
  b32 boarded = state == CWW_BOARDED;
  b32 open = state == CWW_OPEN || state == CWW_AJAR;
  b32 lit = state == CWW_LIT;
  CwGeoId box = BOX_THIN(A);

  if (op->back) {
    f32 bd = depth + (open ? 0.26f : 0.19f);
    cwa_add(A, lit ? CW_WINDOW_GLOW : CW_WINDOW_VOID, PANE(A), LL(pm, x, y, bd, 0, w + 0.14f, h + 0.14f, 1), lit ? CW_M(0.2f, 0.4f, 0.1f) : CW_M(0.15f, 0.9f, 0.95f));
    if (!boarded) {
      cwa_add(A, CW_WINDOW_VOID, box, LL(pm, x - w / 2 - 0.03f, y, bd - 0.09f, 0, 0.05f, h + 0.1f, 0.2f), CW_M(0.1f, 0.95f, 1.0f));
      cwa_add(A, CW_WINDOW_VOID, box, LL(pm, x, y + h / 2 + 0.03f, bd - 0.09f, 0, w + 0.1f, 0.05f, 0.2f), CW_M(0.1f, 0.95f, 1.0f));
    }
  }

  cwa_add(A, frame_key, box, LL(pm, x, y + h / 2 - fw / 2, depth, 0, w - 0.02f, fw, fd), NULL);
  cwa_add(A, frame_key, box, LL(pm, x, y - h / 2 + fw / 2, depth, 0, w - 0.02f, fw, fd), NULL);
  cwa_add(A, frame_key, box, LL(pm, x - w / 2 + fw / 2, y, depth, 0, fw, h - 0.02f, fd), NULL);
  cwa_add(A, frame_key, box, LL(pm, x + w / 2 - fw / 2, y, depth, 0, fw, h - 0.02f, fd), NULL);
  b32 openL = false, openR = false;
  if (!open) {
    cwa_add(A, frame_key, box, LL(pm, x, y, depth, 0, 0.045f, h - 0.1f, fd * 0.85f), NULL);
    cwa_add(A, frame_key, box, LL(pm, x, y + h * 0.16f, depth, 0, w - 0.1f, 0.04f, fd * 0.85f), NULL);
  } else {
    openL = true;
    openR = state == CWW_OPEN || cw_chance(rng, 0.4);
    f32 sw = w / 2 - 0.03f;
    CwGeoId sash = cw_cached_sash(A, sw, h);
    f32 a0 = cod_rng_range(rng, -1.35f, -0.75f);
    cwa_add(A, frame_key, sash, LL(pm, x - w / 2 + 0.04f, y, depth + 0.02f, a0, 1, 1, 1), CW_M(0.8f, 0.45f, 0.2f));
    if (openR) {
      f32 a1 = cod_rng_range(rng, 0.75f, 1.35f);
      cwa_add(A, frame_key, sash, LL(pm, x + w / 2 - 0.04f, y, depth + 0.02f, a1, 1, 1, 1), CW_M(0.8f, 0.45f, 0.2f));
    } else {
      cwa_add(A, frame_key, box, LL(pm, x, y, depth, 0, 0.045f, h - 0.1f, fd * 0.85f), NULL);
    }
  }

  if (boarded) {
    i32 n = cod_rng_int(rng, 3, 5);
    for (i32 i = 0; i < n; i++) {
      f32 bh = (h + 0.05f) / (f32)n;
      f32 jx = cod_rng_range(rng, -0.04f, 0.04f);
      f32 jr = cod_rng_range(rng, -0.02f, 0.02f);
      cwa_add(A, CW_PLYWOOD, box, LLR(pm, x + jx, y - (h + 0.05f) / 2 + ((f32)i + 0.5f) * bh, depth - 0.05f, 0, w - 0.02f, bh - 0.012f, 0.026f, 0, jr),
              CW_M(0.75f, 0.55f, 0.25f));
    }
    if (cw_chance(rng, 0.5))
      cwa_add(A, CW_METAL_RUST, box, LL(pm, x, y + h / 2 - 0.1f, depth - 0.08f, 0, w * 0.5f, 0.03f, 0.02f), CW_M(0.9f, 0.6f, 0.0f));
  }

  if (!boarded) {
    f32 px[4] = {x - w / 4, x + w / 4, x - w / 4, x + w / 4};
    f32 py[4] = {y + h * 0.33f, y + h * 0.33f, y - h * 0.17f, y - h * 0.17f};
    f32 pw = w / 2 - 0.09f;
    f32 ph[4] = {h * 0.3f, h * 0.3f, h * 0.6f, h * 0.6f};
    for (u32 i = 0; i < 4; i++) {
      if (px[i] < x ? openL : openR)
        continue;
      if (broken && cw_chance(rng, 0.55))
        continue;
      cwa_add(A, CW_WINDOW_GLASS, PANE(A), LL(pm, px[i], py[i], depth, 0, pw, ph[i], 1), CW_M(0.1f, 0.3f, 0.0f));
    }
    if (broken) {
      for (u32 i = 0; i < 3; i++) {
        f32 sw = cod_rng_range(rng, 0.08f, 0.26f);
        f32 jx = cod_rng_range(rng, -w / 2 + 0.1f, w / 2 - 0.1f);
        f32 jy = cod_rng_range(rng, 0.4f, 0.9f);
        f32 sy = cod_rng_range(rng, 0.6f, 1.4f);
        f32 rz = cod_rng_range(rng, -0.5f, 0.5f);
        cwa_add(A, CW_GLASS, box, LLR(pm, x + jx, y + h / 2 - sw * jy, depth, 0, sw, sw * sy, 0.01f, 0, rz), NULL);
      }
    }
  }

  if (!op->no_sill)
    cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(pm, x, y - h / 2 - 0.045f, -0.045f, 0, w + 0.26f, 0.09f, t * 0.55f), CW_M(0.5f, 0.35f, 0.2f));
  if (!op->no_lintel)
    cwa_add(A, CW_CONCRETE, BOX(A), LL(pm, x, y + h / 2 + 0.055f, 0.02f, 0, w + 0.18f, 0.11f, t * 0.42f), CW_M(0.35f, 0.5f, 0.3f));

  if (op->grille) {
    u32 n = (u32)m_maxf(3.0f, m_roundf(w / 0.16f));
    for (u32 i = 0; i < n; i++) {
      f32 gx = x - w / 2 + 0.08f + ((f32)i / (f32)(n - 1)) * (w - 0.16f);
      cwa_add(A, CW_METAL_RUST, box, LL(pm, gx, y, 0.055f, 0, 0.022f, h - 0.06f, 0.022f), CW_M(0.8f, 0.5f, 0.0f));
    }
    for (u32 i = 0; i < 2; i++)
      cwa_add(A, CW_METAL_RUST, box, LL(pm, x, y - h / 4 + ((f32)i * h) / 2, 0.055f, 0, w - 0.05f, 0.022f, 0.022f), CW_M(0.8f, 0.5f, 0.0f));
  }

  if (op->shutters) {
    u32 shutter_key = op->shutter_key ? op->shutter_key : CW_METAL_BLUE;
    f32 sw = w / 2 - 0.01f;
    CwGeoId louvre = cw_cached_shutter(A, sw, h);
    b32 shut = state == CWW_SHUTTERED;
    b32 swungL = shut ? false : cw_chance(rng, 0.45);
    b32 swungR = shut ? false : cw_chance(rng, 0.45);
    f32 aL = swungL ? cod_rng_range(rng, 0.9f, 1.5f) : 0.0f;
    cwa_add(A, shutter_key, louvre, LL(pm, x - w / 2 + (swungL ? 0.02f : sw / 2), y, -0.03f, aL, 1, 1, 1), CW_M(0.9f, 0.4f, 0.0f));
    f32 aR = swungR ? -cod_rng_range(rng, 0.9f, 1.5f) : 0.0f;
    cwa_add(A, shutter_key, louvre, LL(pm, x + w / 2 - (swungR ? 0.02f : sw / 2), y, -0.03f, aR, 1, 1, 1), CW_M(0.9f, 0.4f, 0.0f));
  }

  if (op->curtain) {
    CwClothSpec cs = cw_cloth_defaults(w * 0.92f, h * 0.95f);
    cs.seg_x = 7;
    cs.seg_y = 7;
    cs.sag = 0.05f;
    cs.wrinkle = 0.055f;
    cs.twist = 0.05f;
    cs.fray = 0.012f;
    cs.seed = cw_cloth_seed(rng);
    cwa_add(A, op->curtain_key ? op->curtain_key : CW_FABRIC_CREAM, cwa_cloth(A, cs), LL(pm, x + w * 0.03f, y, depth + 0.09f, 0, 1, 1, 1), CW_M(0.1f, 0.35f, 0.1f));
  }
}

// ---- doors ----

typedef struct CwDoorOpts {
  f32 t;
  b32 no_leaf;
  u32 leaf_key;
  f32 open;
  u32 frame_key;
} CwDoorOpts;

hz_internal void cw_door_unit(CwA *A, m4 pm, const CwOpening *o, const CwDoorOpts *op) {
  f32 t = op->t;
  f32 w = o->w, h = o->h, x = o->x, y = o->y;
  CwGeoId box = BOX(A);
  u32 fk = op->frame_key ? op->frame_key : CW_WOOD_DARK;
  cwa_add(A, fk, box, LL(pm, x - w / 2 - 0.03f, y, 0.0f, 0, 0.09f, h + 0.1f, t * 0.9f), CW_M(0.6f, 0.4f, 0.2f));
  cwa_add(A, fk, box, LL(pm, x + w / 2 + 0.03f, y, 0.0f, 0, 0.09f, h + 0.1f, t * 0.9f), CW_M(0.6f, 0.4f, 0.2f));
  cwa_add(A, fk, box, LL(pm, x, y + h / 2 + 0.06f, 0.0f, 0, w + 0.2f, 0.11f, t * 0.9f), CW_M(0.6f, 0.45f, 0.25f));
  cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(pm, x, y - h / 2 + 0.03f, t * 0.5f - 0.02f, 0, w + 0.1f, 0.06f, t), CW_M(0.7f, 0.5f, 0.3f));
  if (!op->no_leaf) {
    f32 ang = op->open;
    f32 leafW = w - 0.06f;
    CwGeoId leaf;
    if (!cwa_cache_find(A, CWK_DOORLEAF, cw_q2(leafW), cw_q2(h), &leaf))
      leaf = cwa_cache_store(A, CWK_DOORLEAF, cw_q2(leafW), cw_q2(h), cwa_ready(A, cw_door_leaf(&A->al, leafW, h - 0.06f)));
    f32 hx = x - leafW / 2;
    f32 cs = m_cosf(ang), sn = m_sinf(ang);
    cwa_add(A, op->leaf_key ? op->leaf_key : CW_METAL_GREEN, leaf, LL(pm, hx + (leafW / 2) * cs, y, t * 0.45f + (leafW / 2) * sn, -ang, 1, 1, 1), CW_M(0.95f, 0.5f, 0.1f));
  }
}

// ---- shopfront ----

hz_internal void cw_shopfront(CwA *A, m4 pm, const CwOpening *o, CodRng *rng, f32 t, f32 drop) {
  f32 x = o->x, y = o->y, w = o->w, h = o->h;
  CwGeoId box = BOX(A);
  cwa_add(A, CW_CONCRETE, box, LL(pm, x, y + h / 2 + 0.11f, t * 0.5f, 0, w + 0.5f, 0.22f, t), CW_M(0.4f, 0.55f, 0.35f));
  cwa_add(A, CW_METAL_DARK, box, LL(pm, x, y + h / 2 - 0.09f, 0.06f, 0, w + 0.12f, 0.18f, 0.16f), CW_M(0.85f, 0.5f, 0.1f));
  if (drop > 0.02f) {
    f32 sh = h * drop;
    CwGeoId shutter;
    if (!cwa_cache_find(A, CWK_ROLLER, cw_q2(w), 0, &shutter))
      shutter = cwa_cache_store(A, CWK_ROLLER, cw_q2(w), 0, cwa_ready(A, cw_roller_shutter(&A->al, w, 1.0f)));
    cwa_add(A, CW_CORRUGATED, shutter, LL(pm, x, y + h / 2 - 0.18f - sh / 2, 0.05f, 0, 1, sh, 1), CW_M(0.85f, 0.6f, 0.15f));
    cwa_slab_box(A, COD_SURFACE_METAL, pm, x, y + h / 2 - 0.18f - sh / 2, w, sh, 0.12f);
  }
  cwa_add(A, CW_WOOD_DARK, box, LL(pm, x, 0.42f, t + 0.28f, 0, w * 0.82f, 0.08f, 0.7f), CW_M(0.8f, 0.5f, 0.2f));
  cwa_add(A, CW_WOOD_DARK, box, LL(pm, x - w * 0.34f, 0.21f, t + 0.28f, 0, 0.09f, 0.42f, 0.62f), CW_M(0.7f, 0.6f, 0.3f));
  cwa_add(A, CW_WOOD_DARK, box, LL(pm, x + w * 0.34f, 0.21f, t + 0.28f, 0, 0.09f, 0.42f, 0.62f), CW_M(0.7f, 0.6f, 0.3f));
  cwa_slab_box(A, COD_SURFACE_WOOD, pm, x, 0.25f, w * 0.82f, 0.5f, t + 0.6f);

  CwGeoId thin = BOX_THIN(A);
  for (i32 si = 0; si < 2; si++) {
    f32 sx = si ? 1.0f : -1.0f;
    f32 bx = x + sx * (w / 2 + 0.75f);
    f32 sy = 1.35f + cod_rng_range(rng, -0.1f, 0.25f);
    cwa_add(A, CW_WOOD_PROP, BOX_FINE(A), LL(pm, bx, sy, t + 0.17f, 0, 1.3f, 0.045f, 0.34f), CW_M(0.85f, 0.4f, 0.15f));
    for (i32 bi = 0; bi < 2; bi++) {
      f32 b = bi ? 1.0f : -1.0f;
      cwa_add(A, CW_METAL_RUST, thin, LL(pm, bx + b * 0.5f, sy - 0.12f, t + 0.09f, 0, 0.03f, 0.24f, 0.18f), CW_M(0.9f, 0.6f, 0.2f));
    }
    cwa_add(A, CW_METAL_DARK, thin, LL(pm, bx + 0.62f, 1.5f, t + 0.03f, 0, 0.045f, 2.6f, 0.045f), CW_M(0.7f, 0.5f, 0.2f));
    cwa_add(A, CW_METAL_DARK, BOX_FINE(A), LL(pm, bx + 0.62f, 1.42f, t + 0.06f, 0, 0.16f, 0.22f, 0.09f), CW_M(0.8f, 0.5f, 0.2f));
  }
  if (cw_chance(rng, 0.7)) {
    CwClothSpec cs = cw_cloth_defaults(0.0f, 0.0f);
    cs.w = cod_rng_range(rng, 0.9f, 1.5f);
    cs.h = cod_rng_range(rng, 1.1f, 1.7f);
    cs.seg_x = 7;
    cs.seg_y = 8;
    cs.sag = 0.05f;
    cs.wrinkle = 0.055f;
    cs.twist = 0.06f;
    cs.thickness = 0.003f;
    cs.fray = 0.02f;
    cs.seed = cw_cloth_seed(rng);
    CwGeoId c = cwa_cloth(A, cs);
    u32 key = cw_pick_fabric3(rng);
    f32 side = cw_chance(rng, 0.5) ? -1.0f : 1.0f;
    f32 off = cod_rng_range(rng, 0.9f, 1.5f);
    cwa_add(A, key, c, LL(pm, x + side * (w / 2 + off), 1.75f, t + 0.08f, CW_PI_F, 1, 1, 1), CW_M(0.3f, 0.45f, 0.2f));
  }
}

// ---- balcony ----

typedef struct CwBalconyOpts {
  f32 depth;
  b32 concrete_rail;
  u32 key;
} CwBalconyOpts;

hz_internal void cw_balcony(CwA *A, m4 pm, f32 x, f32 y, f32 w, const CwBalconyOpts *op) {
  f32 d = op->depth;
  CwGeoId box = BOX(A);
  u32 key = op->key;
  f32 ry = cw_ry_of(pm);
  cwa_add(A, key, BOX_SOFT(A), LL(pm, x, y + 0.06f, -d / 2, 0, w, 0.13f, d), CW_M(0.45f, 0.55f, 0.3f));
  v3 wp = cw_world_of(pm, x, y + 0.06f, -d / 2);
  cwa_box(A, COD_SURFACE_CONCRETE, wp.x, wp.y, wp.z, w, 0.16f, d, ry);
  for (i32 i = -1; i <= 1; i += 2)
    cwa_add(A, key, box, LL(pm, x + (f32)i * (w / 2 - 0.16f), y - 0.14f, -d * 0.42f, 0, 0.11f, 0.3f, d * 0.75f), CW_M(0.4f, 0.6f, 0.4f));
  if (op->concrete_rail) {
    cwa_add(A, key, box, LL(pm, x, y + 0.55f, -d + 0.06f, 0, w, 0.85f, 0.12f), CW_M(0.5f, 0.5f, 0.2f));
    cwa_add(A, key, box, LL(pm, x - w / 2 + 0.06f, y + 0.55f, -d / 2, 0, 0.12f, 0.85f, d), CW_M(0.5f, 0.5f, 0.2f));
    cwa_add(A, key, box, LL(pm, x + w / 2 - 0.06f, y + 0.55f, -d / 2, 0, 0.12f, 0.85f, d), CW_M(0.5f, 0.5f, 0.2f));
    v3 rp = cw_world_of(pm, x, y + 0.55f, -d + 0.06f);
    cwa_box(A, COD_SURFACE_CONCRETE, rp.x, rp.y, rp.z, w, 0.9f, 0.16f, ry);
  } else {
    CwGeoId bar = BOX_THIN(A);
    u32 rk = CW_METAL_RUST;
    cwa_add(A, rk, bar, LL(pm, x, y + 1.0f, -d + 0.04f, 0, w, 0.05f, 0.05f), CW_M(0.9f, 0.45f, 0.0f));
    cwa_add(A, rk, bar, LL(pm, x, y + 0.52f, -d + 0.04f, 0, w, 0.035f, 0.035f), CW_M(0.9f, 0.45f, 0.0f));
    cwa_add(A, rk, bar, LL(pm, x - w / 2, y + 1.0f, -d / 2 + 0.02f, 0, 0.05f, 0.05f, d), CW_M(0.9f, 0.45f, 0.0f));
    cwa_add(A, rk, bar, LL(pm, x + w / 2, y + 1.0f, -d / 2 + 0.02f, 0, 0.05f, 0.05f, d), CW_M(0.9f, 0.45f, 0.0f));
    u32 n = (u32)m_maxf(4.0f, m_roundf(w / 0.17f));
    for (u32 i = 0; i <= n; i++) {
      f32 bx = x - w / 2 + ((f32)i / (f32)n) * w;
      cwa_add(A, rk, bar, LL(pm, bx, y + 0.53f, -d + 0.04f, 0, 0.024f, 1.0f, 0.024f), CW_M(0.9f, 0.5f, 0.0f));
    }
    for (i32 si = 0; si < 2; si++) {
      f32 sx = si ? 1.0f : -1.0f;
      cwa_add(A, rk, bar, LL(pm, x + sx * (w / 2), y + 0.53f, -d + 0.04f, 0, 0.05f, 1.05f, 0.05f), CW_M(0.9f, 0.5f, 0.0f));
      for (u32 i = 0; i <= 3; i++)
        cwa_add(A, rk, bar, LL(pm, x + sx * (w / 2), y + 0.53f, -d + 0.04f + ((f32)i / 3.0f) * (d - 0.08f), 0, 0.024f, 1.0f, 0.024f),
                CW_M(0.9f, 0.5f, 0.0f));
    }
    v3 rp = cw_world_of(pm, x, y + 0.55f, -d + 0.06f);
    cwa_box(A, COD_SURFACE_METAL, rp.x, rp.y, rp.z, w, 0.95f, 0.1f, ry);
  }
}

// ---- parapet ----

// a roof edge wall with a coping course; returns the top y
hz_internal f32 cw_parapet(CwA *A, u32 key, f32 cx, f32 cz, f32 w, f32 d, f32 y, CodRng *rng, f32 h, f32 t) {
  CwGeoId box = BOX(A);
  f32 sx[4] = {cx, cx, cx - w / 2 + t / 2, cx + w / 2 - t / 2};
  f32 sz[4] = {cz - d / 2 + t / 2, cz + d / 2 - t / 2, cz, cz};
  f32 sw[4] = {w, w, t, t};
  f32 sd[4] = {t, t, d, d};
  for (u32 i = 0; i < 4; i++) {
    f32 jitter = cod_rng_range(rng, -0.05f, 0.05f);
    cwa_add(A, key, box, LL(IDENT, sx[i], y + (h + jitter) / 2, sz[i], 0, sw[i], h + jitter, sd[i]), CW_M(0.5f, 0.4f, 0.15f));
    cwa_add(A, CW_CONCRETE, BOX_SOFT(A), LL(IDENT, sx[i], y + h + jitter + 0.045f, sz[i], 0, sw[i] + 0.09f, 0.09f, sd[i] + 0.09f),
            CW_M(0.75f, 0.3f, 0.1f));
    cwa_box(A, COD_SURFACE_CONCRETE, sx[i], y + (h + 0.1f) / 2, sz[i], sw[i], h + 0.1f, sd[i], 0.0f);
  }
  return y + h;
}

// ---- stairs ----

typedef struct CwStairOpts {
  u32 key;
  b32 no_stringer;
  u32 railing; // 0 none, 1 right, 2 left, 3 both
} CwStairOpts;

hz_internal void cw_stair_run(CwA *A, m4 pm, f32 x, f32 y, f32 z, f32 w, u32 steps, f32 rise, f32 run, const CwStairOpts *op) {
  u32 key = op->key;
  CwGeoId box = BOX(A);
  f32 ry = cw_ry_of(pm);
  for (u32 i = 0; i < steps; i++) {
    f32 sy = y + ((f32)i + 0.5f) * rise;
    f32 sz = z + ((f32)i + 0.5f) * run;
    cwa_add(A, key, box, LL(pm, x, sy, sz, 0, w, rise, run), CW_M(0.7f, 0.35f, 0.15f));
    v3 wp = cw_world_of(pm, x, sy, sz);
    cwa_box(A, cw_surface_of(key), wp.x, wp.y, wp.z, w, rise, run, ry);
  }
  f32 H = (f32)steps * rise, D = (f32)steps * run;
  if (!op->no_stringer)
    cwa_add(A, key, box, LL(pm, x, y + H / 2 - 0.1f, z + D / 2, 0, w * 1.02f, H, D * 0.99f), CW_M(0.4f, 0.6f, 0.4f));
  if (op->railing) {
    CwGeoId bar = BOX_THIN(A);
    f32 ang = m_atan2f(H, D);
    f32 len = m_sqrtf(H * H + D * D);
    for (i32 si = 0; si < 2; si++) {
      f32 sx = si ? 1.0f : -1.0f;
      if (op->railing == 1 && sx < 0)
        continue;
      if (op->railing == 2 && sx > 0)
        continue;
      cwa_add(A, CW_METAL_RUST, bar, LLR(pm, x + sx * (w / 2 - 0.05f), y + H / 2 + 0.95f, z + D / 2, 0, 0.045f, 0.045f, len, -ang, 0.0f),
              CW_M(0.9f, 0.5f, 0.0f));
      for (u32 i = 0; i < steps; i += 3)
        cwa_add(A, CW_METAL_RUST, bar, LL(pm, x + sx * (w / 2 - 0.05f), y + (f32)i * rise + 0.5f, z + ((f32)i + 0.5f) * run, 0, 0.03f, 1.0f, 0.03f),
                CW_M(0.9f, 0.5f, 0.0f));
    }
  }
}

// ---- cloth canopies ----

typedef struct CwStripedOpts {
  u32 bands; // 0 = from the width
  u32 seg_x, seg_y;
  f32 sag, wrinkle, bulge, twist, thickness, fray;
  b32 bulge_set;
  i32 skip_band; // band index + 1; 0 = none
  b32 has_masks;
  v3 masks;
} CwStripedOpts;

// one continuous catenary surface split into alternating colour strips
hz_internal void cw_striped_cloth(CwA *A, const u32 *keys, u32 key_count, m4 m, f32 w, f32 h, CwStripedOpts o, CodRng *rng) {
  u32 bands = o.bands ? o.bands : (u32)m_maxf(3.0f, m_roundf(w / 0.38f));
  f32 seed = (rng ? (f32)(cod_rng_f64(rng)) : 0.5f) * 30.0f;
  u32 segX = (u32)m_maxf(2.0f, m_roundf(24.0f / (f32)bands));
  v3 masks = o.has_masks ? o.masks : (v3){0.3f, 0.5f, 0.15f};
  for (u32 i = 0; i < bands; i++) {
    if ((i32)i == o.skip_band - 1)
      continue;
    CwClothSpec cs = cw_cloth_defaults(w, h);
    cs.seg_x = o.seg_x ? o.seg_x : segX;
    cs.seg_y = o.seg_y ? o.seg_y : 6;
    cs.sag = o.sag > 0.0f ? o.sag : 0.14f;
    cs.wrinkle = o.wrinkle > 0.0f ? o.wrinkle : 0.03f;
    cs.bulge = o.bulge_set ? o.bulge : 0.04f;
    cs.twist = o.twist;
    cs.thickness = o.thickness > 0.0f ? o.thickness : 0.0024f;
    cs.fray = o.fray;
    cs.has_range = true;
    cs.u0 = (f32)i / (f32)bands;
    cs.u1 = (f32)(i + 1) / (f32)bands;
    cs.seed = seed;
    CwGeoId g = cwa_cloth(A, cs);
    f32 gv = rng ? cod_rng_range(rng, 0.85f, 1.18f) : 1.0f;
    CwOpts op = {.has_masks = true, .masks = {masks.x, m_minf(1.0f, masks.y * gv), masks.z}};
    cwa_add(A, keys[i % key_count], g, m, &op);
  }
}

typedef struct CwAwningOpts {
  f32 depth;
  u32 key;
  b32 legs;
} CwAwningOpts;

hz_internal void cw_awning(CwA *A, m4 pm, f32 x, f32 y, f32 w, CodRng *rng, const CwAwningOpts *op) {
  f32 d = op->depth;
  f32 slope = 0.32f;
  u32 keys[2] = {op->key, CW_FABRIC_CREAM};
  f32 slack = cod_rng_range(rng, 0.85f, 1.45f);
  CwStripedOpts a = {.seg_y = 6, .sag = 0.11f * slack, .wrinkle = 0.026f * slack, .bulge = 0.055f * slack, .bulge_set = true, .thickness = 0.0026f,
                     .has_masks = true, .masks = {0.2f, 0.45f, 0.15f}};
  cw_striped_cloth(A, keys, 2, LLR(pm, x, y - slope * 0.5f, -d / 2, 0, 1, 1, 1, -CW_PI_F / 2 + slope, 0.0f), w, d, a, rng);
  CwStripedOpts b = {.seg_y = 3, .bulge_set = true, .sag = 0.05f * slack, .wrinkle = 0.026f * slack, .thickness = 0.0026f, .fray = 0.018f, .has_masks = true,
                     .masks = {0.3f, 0.5f, 0.2f}};
  cw_striped_cloth(A, keys, 2, LL(pm, x, y - slope - 0.13f, -d, 0, 1, 1, 1), w, 0.26f, b, rng);
  CwGeoId bar = BOX_THIN(A);
  for (i32 si = 0; si < 2; si++) {
    f32 sx = si ? 1.0f : -1.0f;
    cwa_add(A, CW_METAL_RUST, bar, LLR(pm, x + sx * (w / 2 - 0.05f), y - slope * 0.5f, -d / 2, 0, 0.04f, 0.04f, d, -m_atan2f(slope, d), 0.0f),
            CW_M(0.9f, 0.5f, 0.0f));
    if (op->legs)
      cwa_add(A, CW_METAL_RUST, bar, LL(pm, x + sx * (w / 2 - 0.05f), (y - slope) / 2, -d + 0.05f, 0, 0.045f, y - slope, 0.045f), CW_M(0.9f, 0.55f, 0.0f));
  }
  cwa_add(A, CW_METAL_RUST, bar, LL(pm, x, y - slope, -d + 0.03f, 0, w, 0.04f, 0.04f), CW_M(0.9f, 0.5f, 0.0f));
}

// ---- pipework ----

hz_internal void cw_drainpipe(CwA *A, m4 pm, f32 x, f32 yTop, f32 h) {
  f32 r = 0.055f;
  u32 key = CW_METAL_RUST;
  CwGeoId pipe;
  if (!cwa_cache_find(A, CWK_PIPE, (i32)m_roundf(r * 1000.0f), 0, &pipe))
    pipe = cwa_cache_store(A, CWK_PIPE, (i32)m_roundf(r * 1000.0f), 0, cwa_ready(A, cw_tube_y(&A->al, r, 1.0f, 8, 1.0f, false, 1)));
  f32 z = -r - 0.02f;
  u32 segs = (u32)m_maxf(2.0f, m_roundf(h / 1.6f));
  f32 y = yTop - h;
  for (u32 i = 0; i < segs; i++) {
    f32 sh = h / (f32)segs;
    cwa_add(A, key, pipe, LL(pm, x + (i % 2 ? 0.006f : -0.006f), y, z, 0, 1, sh, 1), CW_M(0.85f, 0.6f, 0.1f));
    cwa_add(A, key, pipe, LL(pm, x, y + sh - 0.03f, z, 0, 1.22f, 0.075f, 1.22f), CW_M(0.9f, 0.7f, 0.2f));
    y += sh;
  }
  cwa_add(A, key, pipe, LLR(pm, x, yTop - h + 0.02f, z + 0.09f, 0, 1, 0.3f, 1, -0.75f, 0.0f), CW_M(0.85f, 0.7f, 0.3f));
  cwa_add(A, key, BOX_FINE(A), LL(pm, x, yTop - 0.09f, z - 0.01f, 0, 0.2f, 0.2f, 0.17f), CW_M(0.85f, 0.65f, 0.25f));
  cwa_add(A, CW_METAL_DARK, BOX_FINE(A), LL(pm, x, yTop + 0.02f, z - 0.01f, 0, 0.24f, 0.03f, 0.2f), CW_M(0.9f, 0.55f, 0.2f));
  cwa_add(A, key, BOX_FINE(A), LL(pm, x, yTop - 0.24f, z * 0.35f, 0, 0.09f, 0.3f, 0.02f), CW_M(0.2f, 1.0f, 0.6f));
  for (u32 i = 1; i < segs; i++)
    cwa_add(A, CW_METAL_DARK, BOX_FINE(A), LL(pm, x, yTop - h + ((f32)i * h) / (f32)segs, z * 0.45f, 0, 0.16f, 0.03f, m_fabsf(z) * 0.9f),
            CW_M(0.9f, 0.6f, 0.2f));
}

// ---- damage ----

// bullet pock: a shallow crater with a raised, chipped rim; draws the rng exactly 16 times
hz_internal CwGeo cw_pock(Allocator *al, CodRng *rng, f32 r) {
  const u32 SEG = 8;
  const f32 rf[4] = {0.0f, 0.42f, 0.8f, 1.0f};
  const f32 zf[4] = {0.010f, 0.024f, 0.075f, 0.004f};
  f32 jr[8], jz[8];
  for (u32 s = 0; s < SEG; s++)
    jr[s] = 1.0f + (cod_rng_float(rng) - 0.5f) * 0.42f;
  for (u32 s = 0; s < SEG; s++)
    jz[s] = 0.62f + cod_rng_float(rng) * 0.76f;
  CwGeo g = cw_geo_new(al, 1 + 3 * SEG, 3 * SEG + 2 * SEG * 6);
  cw_geo_vert(&g, (v3){0, 0, zf[0] * r}, V3_ZERO, (v2){0, 0}, V3_ZERO);
  for (u32 k = 1; k < 4; k++) {
    for (u32 s = 0; s < SEG; s++) {
      f32 a = ((f32)s / (f32)SEG) * (f32)CW_TAU;
      f32 rj = k >= 2 ? jr[s] : 1.0f;
      f32 zj = k == 2 ? jz[s] : 1.0f;
      cw_geo_vert(&g, (v3){m_cosf(a) * rf[k] * r * rj, m_sinf(a) * rf[k] * r * rj, zf[k] * r * zj}, V3_ZERO, (v2){0, 0}, V3_ZERO);
    }
  }
  for (u32 s = 0; s < SEG; s++)
    cw_geo_tri(&g, 0, 1 + s, 1 + (s + 1) % SEG);
  for (u32 k = 1; k < 3; k++) {
    u32 a0 = 1 + (k - 1) * SEG, b0 = 1 + k * SEG;
    for (u32 s = 0; s < SEG; s++) {
      u32 n = (s + 1) % SEG;
      cw_geo_tri(&g, a0 + s, b0 + s, b0 + n);
      cw_geo_tri(&g, a0 + s, b0 + n, a0 + n);
    }
  }
  cw_geo_normals(&g);
  for (u32 i = 0; i < g.nv; i++) {
    f32 t = m_minf(1.0f, m_sqrtf(g.pos[i].x * g.pos[i].x + g.pos[i].y * g.pos[i].y) / (r * 0.8f));
    g.col[i] = (v3){0.9f - 0.35f * t, 0.62f - 0.3f * t, 0.9f - 0.55f * t};
  }
  return g;
}

// a low pile of masonry chunks and its collision hull
hz_internal void cw_rubble_mound(CwA *A, CodRng *rng, f32 x, f32 y, f32 z, f32 radius, u32 count, u32 key) {
  for (u32 i = 0; i < count; i++) {
    f32 a = cod_rng_float(rng) * CW_PI_F * 2.0f;
    f32 rr = m_sqrtf(cod_rng_float(rng)) * radius;
    f32 s = cod_rng_range(rng, 0.09f, 0.3f) * (1.0f - rr / radius / 1.6f);
    CwGeoId g = cwa_rock(A, rng, s, 0, 0.75f);
    f32 ry = cod_rng_float(rng) * 6.28f;
    f32 rx = cod_rng_range(rng, -0.4f, 0.4f);
    f32 rz = cod_rng_range(rng, -0.4f, 0.4f);
    cwa_add(A, key, g,
            LLR(IDENT, x + m_cosf(a) * rr, y + s * 0.3f + m_maxf(0.0f, (1.0f - rr / radius) * radius * 0.3f), z + m_sinf(a) * rr, ry, 1, 1, 1, rx, rz),
            CW_M(0.3f, 0.75f, 0.45f));
  }
  cwa_box(A, cw_surface_of(key), x, y + radius * 0.14f, z, radius * 1.5f, radius * 0.34f, radius * 1.5f, 0.0f);
}
