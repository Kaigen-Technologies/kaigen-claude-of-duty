// Claude of Duty world primitives the prop library adds to the geometry toolkit: part builder, plane, lathe, torus, dish.

// a plane (w, h, sx, sy): XY facing +Z, uv 0..1
hz_internal CwGeo cw_plane(Allocator *al, f32 w, f32 h, u32 gx, u32 gy) {
  CwGeo g = cw_geo_new(al, (gx + 1) * (gy + 1), gx * gy * 6);
  g.has_col = false;
  f32 w2 = w * 0.5f, h2 = h * 0.5f;
  f32 sw = w / (f32)gx, sh = h / (f32)gy;
  for (u32 iy = 0; iy <= gy; iy++) {
    f32 y = (f32)iy * sh - h2;
    for (u32 ix = 0; ix <= gx; ix++) {
      f32 x = (f32)ix * sw - w2;
      cw_geo_vert(&g, (v3){x, -y, 0.0f}, (v3){0, 0, 1}, (v2){(f32)ix / (f32)gx, 1.0f - (f32)iy / (f32)gy}, V3_ZERO);
    }
  }
  u32 row = gx + 1;
  for (u32 iy = 0; iy < gy; iy++) {
    for (u32 ix = 0; ix < gx; ix++) {
      u32 a = ix + row * iy, b = ix + row * (iy + 1), c = ix + 1 + row * (iy + 1), d = ix + 1 + row * iy;
      cw_geo_tri(&g, a, b, d);
      cw_geo_tri(&g, b, c, d);
    }
  }
  return g;
}

// vertices within `margin` of two or more bounding faces get `amount` wear
typedef struct CwEdgeCtx {
  aabb bb;
  v3 size;
  f32 margin, amount;
} CwEdgeCtx;

hz_internal void cw_edge_wear_fn(const void *ctx, v3 p, v3 n, f32 *out) {
  (void)n;
  const CwEdgeCtx *c = ctx;
  u32 nearc = 0;
  if (c->size.x > c->margin * 3 && (p.x - c->bb.min.x < c->margin || c->bb.max.x - p.x < c->margin))
    nearc++;
  if (c->size.y > c->margin * 3 && (p.y - c->bb.min.y < c->margin || c->bb.max.y - p.y < c->margin))
    nearc++;
  if (c->size.z > c->margin * 3 && (p.z - c->bb.min.z < c->margin || c->bb.max.z - p.z < c->margin))
    nearc++;
  if (nearc >= 2)
    out[0] = m_maxf(out[0], c->amount);
}

hz_internal void cw_auto_edge_wear(CwGeo *g, f32 margin, f32 amount) {
  CwEdgeCtx c = {.bb = cw_geo_bounds(g), .margin = margin, .amount = amount};
  c.size = v3_sub(c.bb.max, c.bb.min);
  if (!g->has_col)
    for (u32 i = 0; i < g->nv; i++)
      g->col[i] = V3_ZERO;
  cw_geo_paint(g, cw_edge_wear_fn, &c);
}

// a part-merging builder
typedef struct CwPB {
  Allocator *al;
  CwGeo g;
} CwPB;

typedef struct CwPBO {
  f32 bevel, ry, rx, rz; // bevel 0 -> 0.008
  f32 wear, grime, ao;   // wear 0 -> 1
  f32 taper;             // taper 0 -> 1
  u32 radial, seg;       // radial 0 -> 12, seg 0 -> 1
  b32 open;
  f32 margin; // 0 -> min(r, h) * 0.12
  f32 sx, sy, sz; // 0 -> 1
  b32 no_auto_wear;
} CwPBO;

hz_internal CwPB cw_pb_begin(Allocator *al, u32 max_verts, u32 max_idx) {
  return (CwPB){.al = al, .g = cw_geo_new(al, max_verts, max_idx)};
}

// adds a part: scale wear, floor grime and ao
hz_internal void cw_pb_push(CwPB *pb, CwGeo *part, const CwPBO *o) {
  if (!part->has_col)
    cw_geo_fill_masks(part, 0.2f, 0.0f, 0.0f);
  f32 wear = o->wear > 0.0f ? o->wear : 1.0f;
  if (wear != 1.0f || o->grime > 0.0f || o->ao > 0.0f) {
    for (u32 i = 0; i < part->nv; i++) {
      v3 c = part->col[i];
      part->col[i] = (v3){m_minf(1.0f, c.x * wear), m_minf(1.0f, m_maxf(c.y, o->grime)), m_minf(1.0f, m_maxf(c.z, o->ao))};
    }
  }
  cw_geo_append(&pb->g, part);
}

hz_internal void cw_pb_box(CwPB *pb, f32 sx, f32 sy, f32 sz, f32 x, f32 y, f32 z, CwPBO o) {
  tctx_temp_allocator_scope(ta, allocator_as_arena(pb->al)) {
    CwGeo g = cw_chamfer_box(&ta.allocator, sx, sy, sz, o.bevel > 0.0f ? o.bevel : 0.008f);
    cw_geo_apply(&g, cw_trs(x, y, z, o.ry, 1, 1, 1, o.rx, o.rz));
    cw_pb_push(pb, &g, &o);
  }
}

hz_internal void cw_pb_cyl(CwPB *pb, f32 r, f32 h, f32 x, f32 y, f32 z, CwPBO o) {
  tctx_temp_allocator_scope(ta, allocator_as_arena(pb->al)) {
    CwGeo g = cw_cylinder(&ta.allocator, (o.taper > 0.0f ? o.taper : 1.0f) * r, r, h, o.radial ? o.radial : 12, o.seg ? o.seg : 1, o.open);
    cw_auto_edge_wear(&g, o.margin > 0.0f ? o.margin : m_minf(r, h) * 0.12f, 0.9f);
    cw_geo_apply(&g, cw_trs(x, y, z, o.ry, 1, 1, 1, o.rx, o.rz));
    cw_pb_push(pb, &g, &o);
  }
}

// adds a geometry: `g` is consumed (transformed in place)
hz_internal void cw_pb_geo(CwPB *pb, CwGeo *g, f32 x, f32 y, f32 z, CwPBO o) {
  if (!o.no_auto_wear && !g->has_col)
    cw_auto_edge_wear(g, o.margin > 0.0f ? o.margin : 0.02f, 1.0f);
  cw_geo_apply(g, cw_trs(x, y, z, o.ry, o.sx > 0.0f ? o.sx : 1.0f, o.sy > 0.0f ? o.sy : 1.0f, o.sz > 0.0f ? o.sz : 1.0f, o.rx, o.rz));
  cw_pb_push(pb, g, &o);
}

// the merged result, copied exact-size onto `al`
hz_internal CwGeo cw_pb_build(CwPB *pb, Allocator *al) {
  CwGeo g = cw_geo_new(al, pb->g.nv, pb->g.ni);
  cw_geo_append(&g, &pb->g);
  return g;
}

hz_internal CwGeo cw_geo_copy(Allocator *al, const CwGeo *src) {
  CwGeo g = cw_geo_new(al, src->nv, src->ni);
  cw_geo_append(&g, src);
  g.has_col = src->has_col;
  return g;
}

// a lathe (points, segments): profile (radius, y) revolved about Y
hz_internal CwGeo cw_lathe(Allocator *al, const v2 *pts, u32 count, u32 segments) {
  CwGeo g;
  tctx_temp_allocator_scope(ta, allocator_as_arena(al)) {
    PgMesh m;
    pg_mesh_begin(&m, ta.temp.arena, 256, 1024);
    pg_revolve(&m, pts, count, segments, 0.0f, 0.0f);
    g = cw_geo_from_pg(al, &m);
  }
  return g;
}

hz_internal CwGeo cw_torus(Allocator *al, f32 radius, f32 tube, u32 radial, u32 tubular) {
  CwGeo g;
  tctx_temp_allocator_scope(ta, allocator_as_arena(al)) {
    PgMesh m;
    pg_mesh_begin(&m, ta.temp.arena, 256, 1024);
    pg_torus(&m, &(PgTorusDesc){.radius = radius, .tube = tube, .radial_segments = radial, .tubular_segments = tubular});
    g = cw_geo_from_pg(al, &m);
  }
  return g;
}

hz_internal CwGeo cw_sphere(Allocator *al, f32 radius, u32 wseg, u32 hseg, f32 theta_length) {
  CwGeo g;
  tctx_temp_allocator_scope(ta, allocator_as_arena(al)) {
    PgMesh m;
    pg_mesh_begin(&m, ta.temp.arena, 256, 1024);
    pg_sphere(&m, &(PgSphereDesc){.radius = radius, .width_segments = wseg, .height_segments = hseg, .theta_length = theta_length});
    g = cw_geo_from_pg(al, &m);
  }
  return g;
}

// ExtrudeGeometry of a closed polygon (x, y) along +Z with a one-segment bevel, flat normals
hz_internal CwGeo cw_extrude_poly(Allocator *al, const v2 *pts, u32 n, f32 depth, f32 bevel) {
  CwGeo g;
  tctx_temp_allocator_scope(ta, allocator_as_arena(al)) {
    PgShape shape;
    pg_shape_begin(&shape, &ta.allocator);
    pg_path2_move_to(&shape.path, pts[0].x, pts[0].y);
    for (u32 i = 1; i < n; i++)
      pg_path2_line_to(&shape.path, pts[i].x, pts[i].y);
    pg_path2_close(&shape.path);
    PgMesh m;
    pg_mesh_begin(&m, ta.temp.arena, 256, 1024);
    pg_extrude_shape(&m, &shape, 1,
                     &(PgExtrudeShapeDesc){.depth = depth, .bevel_enabled = bevel > 0.0f, .bevel_thickness = bevel, .bevel_size = bevel,
                                           .bevel_segments = 1, .steps = 1});
    g = cw_geo_from_pg(al, &m);
  }
  return g;
}
