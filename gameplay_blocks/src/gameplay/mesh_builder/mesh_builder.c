#include "mesh_builder.h"
#include "lib/color.h"

v4 mesh_builder_hex(u32 hex) {
  return (v4){srgb_to_linear((f32)((hex >> 16) & 0xFF) / 255.0f), srgb_to_linear((f32)((hex >> 8) & 0xFF) / 255.0f),
              srgb_to_linear((f32)(hex & 0xFF) / 255.0f), 1.0f};
}

// shortest-arc rotation between two unit vectors, including its antipodal axis choice
hz_internal quat mb_quat_from_unit(v3 f, v3 t) {
  f32 r = v3_dot(f, t) + 1.0f;
  quat q;
  if (r < 1e-8f) {
    r = 0.0f;
    q = m_fabsf(f.x) > m_fabsf(f.z) ? (quat){-f.y, f.x, 0.0f, r} : (quat){0.0f, -f.z, f.y, r};
  } else {
    q = (quat){f.y * t.z - f.z * t.y, f.z * t.x - f.x * t.z, f.x * t.y - f.y * t.x, r};
  }
  return quat_normalize(q);
}

m4 mesh_builder_segment(v3 from, v3 to, f32 *out_len) {
  v3 d = v3_sub(to, from);
  f32 len = v3_length(d);
  if (out_len)
    *out_len = len;
  return m4_trs(v3_scale(v3_add(from, to), 0.5f), mb_quat_from_unit(V3_UP, len > 1e-12f ? v3_scale(d, 1.0f / len) : V3_UP),
                V3_ONE);
}

hz_internal m3 mb_upper(m4 x) {
  m3 u;
  for (u32 c = 0; c < 3; c++)
    for (u32 r = 0; r < 3; r++)
      u.m[c][r] = x.m[c][r];
  return u;
}

// translation, rotation and the absolute per-axis scale of an affine transform
hz_internal void mb_pose(m4 x, v3 *pos, quat *rot, v3 *scale) {
  v4 t;
  m4 r;
  v3 s;
  m4_decompose(x, &t, &r, &s);
  *pos = (v3){t.x, t.y, t.z};
  *rot = quat_normalize(quat_from_m3(mb_upper(r)));
  *scale = (v3){m_fabsf(s.x), m_fabsf(s.y), m_fabsf(s.z)};
}

hz_internal void mb_tmp_reset(MeshBuilder *b) {
  PgMesh *t = &b->tmp;
  t->pos.len = t->nrm.len = t->uv.len = t->mask.len = t->uv1.len = t->idx.len = t->charts.len = 0;
}

void mesh_builder_begin(MeshBuilder *b, ArenaAllocator *arena, const MeshBuilderDesc *d) {
  assert_msg(d->slot_count > 0, "mesh_builder_begin: slot_count is 0");
  *b = (MeshBuilder){.arena = arena,
                     .alloc = make_arena_allocator(arena),
                     .slot_count = d->slot_count,
                     .streams = d->streams,
                     .vtx_reserve = d->vtx_reserve ? d->vtx_reserve : 1024,
                     .idx_reserve = d->idx_reserve ? d->idx_reserve : 4096};
  b->slots = ALLOC_ARRAY(&b->alloc, PgMesh, d->slot_count);
  b->used = ALLOC_ARRAY(&b->alloc, b32, d->slot_count);
  pg_mesh_begin(&b->tmp, arena, 1024, 4096);
  b->colliders = dyn_arr_new_alloc(&b->alloc, MeshBuilderColliderDesc, 16);
}

PgMesh *mesh_builder_slot_mesh(MeshBuilder *b, u32 slot) {
  assert_msg(slot < b->slot_count, "mesh_builder: slot % past the % the builder was begun with", fmt_u32(slot),
             fmt_u32(b->slot_count));
  if (!b->used[slot]) {
    pg_mesh_begin(&b->slots[slot], b->arena, b->vtx_reserve, b->idx_reserve);
    if (b->streams)
      pg_mesh_enable_streams(&b->slots[slot], b->streams);
    b->used[slot] = true;
  }
  return &b->slots[slot];
}

// consumes `t` (local space) under xform into the style's slot
hz_internal void mb_add(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, const PgMesh *t) {
  PgMesh *o = mesh_builder_slot_mesh(b, st->slot);
  b32 shaded = st->shade_lo != 0.0f || st->shade_hi != 0.0f;
  u32 need = (b->streams | t->streams) & (PG_STREAM_COLOR | PG_STREAM_AUX | PG_STREAM_OUTLINE);
  if (st->color.w > 0.0f || st->paint_weight || shaded)
    need |= PG_STREAM_COLOR;
  if (st->aux || st->aux_fn)
    need |= PG_STREAM_AUX;
  pg_mesh_enable_streams(o, need);

  m3 nrm_mat = m3_transpose(m3_inv(mb_upper(xform)));
  u32 first = o->pos.len;
  b32 src_col = (t->streams & PG_STREAM_COLOR) != 0, src_aux = (t->streams & PG_STREAM_AUX) != 0;
  if (st->flat) {
    for (u32 k = 0; k + 2 < t->idx.len; k += 3) {
      v3 a = t->pos.items[t->idx.items[k]], bb = t->pos.items[t->idx.items[k + 1]];
      v3 c = t->pos.items[t->idx.items[k + 2]];
      v3 fn = v3_normalize(v3_cross(v3_sub(c, bb), v3_sub(a, bb)));
      for (u32 e = 0; e < 3; e++) {
        u32 src = t->idx.items[k + e];
        u32 dst = pg_mesh_push_vertex(o, t->pos.items[src], fn, t->uv.items[src]);
        if (src_col)
          o->col.items[dst] = t->col.items[src];
        if (src_aux)
          o->aux.items[dst] = t->aux.items[src];
      }
      pg_mesh_push_tri(o, first + k, first + k + 1, first + k + 2);
    }
  } else {
    for (u32 i = 0; i < t->pos.len; i++) {
      u32 dst = pg_mesh_push_vertex(o, t->pos.items[i], t->nrm.items[i], t->uv.items[i]);
      if (src_col)
        o->col.items[dst] = t->col.items[i];
      if (src_aux)
        o->aux.items[dst] = t->aux.items[i];
    }
    for (u32 k = 0; k + 2 < t->idx.len; k += 3)
      pg_mesh_push_tri(o, first + t->idx.items[k], first + t->idx.items[k + 1], first + t->idx.items[k + 2]);
  }

  f32 min_y = 1e30f, max_y = -1e30f;
  for (u32 i = first; i < o->pos.len; i++) {
    v3 p = m4_mulv3(xform, o->pos.items[i], 1.0f);
    if (st->deform)
      st->deform(&p, st->ctx);
    o->pos.items[i] = p;
    o->nrm.items[i] = v3_normalize(m3_mulv3(nrm_mat, o->nrm.items[i]));
    min_y = m_minf(min_y, p.y);
    max_y = m_maxf(max_y, p.y);
  }
  if (o->streams & PG_STREAM_OUTLINE)
    pg_mesh_outline_normals(o, first, 1e-4f);

  if (!(need & (PG_STREAM_COLOR | PG_STREAM_AUX)))
    return;
  for (u32 i = first; i < o->pos.len; i++) {
    v3 p = o->pos.items[i];
    v3 sn = (o->streams & PG_STREAM_OUTLINE) ? o->outline.items[i] : o->nrm.items[i];
    if (st->color.w > 0.0f || st->paint_weight || shaded) {
      f32 h = max_y > min_y ? (p.y - min_y) / (max_y - min_y) : 1.0f;
      f32 m = shaded ? st->shade_lo + (st->shade_hi - st->shade_lo) * h : 1.0f;
      v4 c = st->color.w > 0.0f ? st->color : o->col.items[i];
      if (st->paint_weight)
        c = v4_lerp(c, st->paint, m_clampf(st->paint_weight(p, sn, st->ctx), 0.0f, 1.0f));
      o->col.items[i] = (v4){c.x * m, c.y * m, c.z * m, c.w};
    }
    if (st->aux_fn)
      o->aux.items[i] = st->aux_fn(p, sn, st->aux, st->ctx);
    else if (st->aux)
      o->aux.items[i] = st->aux;
  }
}

void mesh_builder_add_pg(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, const PgMesh *src) {
  if (st->collider != MESH_BUILDER_COLLIDER_NONE)
    log_error("mesh_builder_add_pg: a collider needs a primitive; use mesh_builder_collider_* for slot %", fmt_u32(st->slot));
  mb_add(b, st, xform, src);
}

hz_internal b32 mb_wants_collider(const MeshBuilderStyle *st, MeshBuilderColliderKind expect, CString who) {
  if (st->collider == MESH_BUILDER_COLLIDER_NONE)
    return false;
  if (st->collider != expect) {
    log_error("mesh_builder_%: collider kind % does not fit the primitive", fmt_cstr(who), fmt_u32((u32)st->collider));
    return false;
  }
  return true;
}

hz_internal void mb_push_collider(MeshBuilder *b, MeshBuilderColliderDesc c) { dyn_arr_append(b->colliders, c); }

void mesh_builder_box(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, v3 size) {
  pg_box_grid(&b->tmp, size, 1, 1, 1);
  mb_add(b, st, xform, &b->tmp);
  mb_tmp_reset(b);
  if (!mb_wants_collider(st, MESH_BUILDER_COLLIDER_BOX, "box"))
    return;
  v3 pos, scale;
  quat rot;
  mb_pose(xform, &pos, &rot, &scale);
  mb_push_collider(b, (MeshBuilderColliderDesc){.kind = MESH_BUILDER_COLLIDER_BOX,
                                                .center = pos,
                                                .rot = rot,
                                                .half = v3_mul(v3_scale(size, 0.5f), scale)});
}

void mesh_builder_round_box(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, v3 size, f32 bevel) {
  pg_round_box(&b->tmp, size, 2, bevel);
  mb_add(b, st, xform, &b->tmp);
  mb_tmp_reset(b);
  if (!mb_wants_collider(st, MESH_BUILDER_COLLIDER_BOX, "round_box"))
    return;
  v3 pos, scale;
  quat rot;
  mb_pose(xform, &pos, &rot, &scale);
  mb_push_collider(b, (MeshBuilderColliderDesc){.kind = MESH_BUILDER_COLLIDER_BOX,
                                                .center = pos,
                                                .rot = rot,
                                                .half = v3_mul(v3_scale(size, 0.5f), scale)});
}

void mesh_builder_sphere(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, u32 width_segments, u32 height_segments) {
  pg_sphere(&b->tmp, &(PgSphereDesc){.radius = 1.0f,
                                     .width_segments = width_segments ? width_segments : 8,
                                     .height_segments = height_segments ? height_segments : 6});
  mb_add(b, st, xform, &b->tmp);
  mb_tmp_reset(b);
  if (!mb_wants_collider(st, MESH_BUILDER_COLLIDER_SPHERE, "sphere"))
    return;
  v3 pos, scale;
  quat rot;
  mb_pose(xform, &pos, &rot, &scale);
  mb_push_collider(b, (MeshBuilderColliderDesc){.kind = MESH_BUILDER_COLLIDER_SPHERE,
                                                .center = pos,
                                                .radius = m_maxf(scale.x, m_maxf(scale.y, scale.z))});
}

void mesh_builder_polyhedron(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, PgPolyhedronKind kind, u32 detail) {
  pg_polyhedron(&b->tmp, kind, 1.0f, detail);
  mb_add(b, st, xform, &b->tmp);
  mb_tmp_reset(b);
  if (mb_wants_collider(st, MESH_BUILDER_COLLIDER_SPHERE, "polyhedron")) {
    v3 pos, scale;
    quat rot;
    mb_pose(xform, &pos, &rot, &scale);
    mb_push_collider(b, (MeshBuilderColliderDesc){.kind = MESH_BUILDER_COLLIDER_SPHERE,
                                                  .center = pos,
                                                  .radius = m_maxf(scale.x, m_maxf(scale.y, scale.z))});
  }
}

void mesh_builder_capsule(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, f32 radius, f32 length, u32 cap_segments,
                          u32 radial_segments) {
  pg_capsule(&b->tmp, radius, length, cap_segments ? cap_segments : 2, radial_segments ? radial_segments : 6, 1);
  mb_add(b, st, xform, &b->tmp);
  mb_tmp_reset(b);
  if (!mb_wants_collider(st, MESH_BUILDER_COLLIDER_CAPSULE, "capsule"))
    return;
  v3 pos, scale;
  quat rot;
  mb_pose(xform, &pos, &rot, &scale);
  v3 half_axis = quat_rotate(rot, (v3){0.0f, length * 0.5f * scale.y, 0.0f});
  mb_push_collider(b, (MeshBuilderColliderDesc){.kind = MESH_BUILDER_COLLIDER_CAPSULE,
                                                .a = v3_sub(pos, half_axis),
                                                .b = v3_add(pos, half_axis),
                                                .radius = radius * m_maxf(scale.x, scale.z)});
}

void mesh_builder_capsule_between(MeshBuilder *b, const MeshBuilderStyle *st, v3 from, v3 to, f32 radius, u32 cap_segments,
                                  u32 radial_segments) {
  f32 len;
  m4 x = mesh_builder_segment(from, to, &len);
  mesh_builder_capsule(b, st, x, radius, m_maxf(len, 1e-3f), cap_segments, radial_segments);
}

void mesh_builder_cylinder(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, const PgCylinderDesc *d) {
  PgCylinderDesc desc = *d;
  if (!desc.height_segments)
    desc.height_segments = 1;
  pg_cylinder(&b->tmp, &desc);
  mb_add(b, st, xform, &b->tmp);
  mb_tmp_reset(b);
  if (!mb_wants_collider(st, MESH_BUILDER_COLLIDER_CYLINDER, "cylinder"))
    return;
  v3 pos, scale;
  quat rot;
  mb_pose(xform, &pos, &rot, &scale);
  f32 radial = m_maxf(scale.x, scale.z);
  mb_push_collider(b, (MeshBuilderColliderDesc){.kind = MESH_BUILDER_COLLIDER_CYLINDER,
                                                .center = pos,
                                                .rot = rot,
                                                .radius_top = d->radius_top * radial,
                                                .radius_bottom = d->radius_bottom * radial,
                                                .height = d->height * scale.y,
                                                .segments = st->collider_segments ? st->collider_segments : d->radial_segments});
}

void mesh_builder_cylinder_between(MeshBuilder *b, const MeshBuilderStyle *st, v3 from, v3 to, f32 radius_from, f32 radius_to,
                                   u32 radial_segments) {
  f32 len;
  m4 x = mesh_builder_segment(from, to, &len);
  mesh_builder_cylinder(b, st, x,
                        &(PgCylinderDesc){.radius_top = radius_to,
                                          .radius_bottom = radius_from,
                                          .height = len,
                                          .radial_segments = radial_segments ? radial_segments : 8,
                                          .height_segments = 1});
}

void mesh_builder_cone_between(MeshBuilder *b, const MeshBuilderStyle *st, v3 base, v3 tip, f32 radius, u32 radial_segments) {
  mesh_builder_cylinder_between(b, st, base, tip, radius, 0.0f, radial_segments ? radial_segments : 6);
}

void mesh_builder_lathe(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, const v2 *profile, u32 count, u32 segments) {
  pg_revolve(&b->tmp, profile, count, segments ? segments : 12, 0.0f, 0.0f);
  mb_add(b, st, xform, &b->tmp);
  mb_tmp_reset(b);
}

void mesh_builder_torus(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, const PgTorusDesc *d) {
  pg_torus(&b->tmp, d);
  mb_add(b, st, xform, &b->tmp);
  mb_tmp_reset(b);
}

void mesh_builder_sheet(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, PgSheetFn surface, void *ctx, f32 thickness,
                        u32 seg_u, u32 seg_v) {
  pg_sheet(&b->tmp, surface, ctx, thickness, seg_u, seg_v);
  mb_add(b, st, xform, &b->tmp);
  mb_tmp_reset(b);
}

void mesh_builder_collider_box(MeshBuilder *b, v3 center, quat rot, v3 half) {
  mb_push_collider(b, (MeshBuilderColliderDesc){.kind = MESH_BUILDER_COLLIDER_BOX, .center = center, .rot = rot, .half = half});
}

void mesh_builder_collider_cylinder(MeshBuilder *b, v3 center, quat rot, f32 radius_top, f32 radius_bottom, f32 height,
                                    u32 segments) {
  mb_push_collider(b, (MeshBuilderColliderDesc){.kind = MESH_BUILDER_COLLIDER_CYLINDER,
                                                .center = center,
                                                .rot = rot,
                                                .radius_top = radius_top,
                                                .radius_bottom = radius_bottom,
                                                .height = height,
                                                .segments = segments});
}

void mesh_builder_collider_sphere(MeshBuilder *b, v3 center, f32 radius) {
  mb_push_collider(b, (MeshBuilderColliderDesc){.kind = MESH_BUILDER_COLLIDER_SPHERE, .center = center, .radius = radius});
}

void mesh_builder_collider_capsule(MeshBuilder *b, v3 a, v3 b_end, f32 radius) {
  mb_push_collider(b, (MeshBuilderColliderDesc){.kind = MESH_BUILDER_COLLIDER_CAPSULE, .a = a, .b = b_end, .radius = radius});
}

// the static bodies: boxes, spheres and capsules directly, cylinders as trimeshes (identical ones share a blob)
hz_internal void mb_spawn_colliders(MeshBuilder *b, const MeshBuilderFinishDesc *d) {
  u32 n = b->colliders.len;
  assert_msg(d->world, "mesh_builder_finish: % colliders were added but the finish desc has no world", fmt_u32(n));
  TempAllocator ta = tctx_temp_allocator_begin(b->arena);
  ColliderBody *bodies = ALLOC_ARRAY(&ta.allocator, ColliderBody, n);
  TriMeshColliderBlob **blobs = ALLOC_ARRAY(&ta.allocator, TriMeshColliderBlob *, n);
  for (u32 i = 0; i < n; i++) {
    const MeshBuilderColliderDesc *c = &b->colliders.items[i];
    bodies[i] = (ColliderBody){.pos = c->center, .rot = quat_identity(), .scale = 1.0f, .filter = d->collider_filter};
    switch (c->kind) {
    case MESH_BUILDER_COLLIDER_BOX:
      bodies[i].rot = c->rot;
      bodies[i].collider = collider_box(V3_ZERO, c->half);
      break;
    case MESH_BUILDER_COLLIDER_SPHERE:
      bodies[i].collider = collider_sphere(V3_ZERO, c->radius);
      break;
    case MESH_BUILDER_COLLIDER_CAPSULE:
      bodies[i].pos = c->a;
      bodies[i].collider = collider_capsule(V3_ZERO, v3_sub(c->b, c->a), c->radius);
      break;
    case MESH_BUILDER_COLLIDER_CYLINDER:
      bodies[i].rot = c->rot;
      for (u32 j = 0; j < i && !blobs[i]; j++) {
        const MeshBuilderColliderDesc *o = &b->colliders.items[j];
        if (blobs[j] && o->radius_top == c->radius_top && o->radius_bottom == c->radius_bottom && o->height == c->height &&
            o->segments == c->segments)
          blobs[i] = blobs[j];
      }
      if (!blobs[i]) {
        PgMesh m;
        pg_mesh_begin(&m, ta.temp.arena, 64, 256);
        pg_cylinder(&m, &(PgCylinderDesc){.radius_top = c->radius_top,
                                          .radius_bottom = c->radius_bottom,
                                          .height = c->height,
                                          .radial_segments = c->segments,
                                          .height_segments = 1});
        i32 *idx = ALLOC_ARRAY(&ta.allocator, i32, m.idx.len);
        for (u32 k = 0; k < m.idx.len; k++)
          idx[k] = (i32)m.idx.items[k];
        blobs[i] = trimesh_blob_build(m.pos.items, idx, (i32)(m.idx.len / 3), d->alloc);
      }
      bodies[i].collider = collider_trimesh(blobs[i]);
      break;
    default:
      break;
    }
  }
  StaticBody tag = {0};
  EcsSpawnColumn *cols = ALLOC_ARRAY(&ta.allocator, EcsSpawnColumn, 2);
  cols[0] = (EcsSpawnColumn){.component = ecs_component_ensure(d->world, sizeof(ColliderBody), _Alignof(ColliderBody), "ColliderBody"),
                             .data = bodies};
  cols[1] = (EcsSpawnColumn){.component = ecs_component_ensure(d->world, sizeof(StaticBody), _Alignof(StaticBody), "StaticBody"),
                             .data = &tag,
                             .constant = true};
  ecs_spawn_block(d->world, &(EcsSpawnBlockDesc){.count = n, .columns = cols, .column_count = 2});
  tctx_temp_allocator_end(ta);
}

MeshBuilderResult mesh_builder_finish(MeshBuilder *b, const MeshBuilderFinishDesc *d) {
  // renderer uploads and the ECS world outside a scope are main-lane state
  assert(is_main_thread());
  MeshBuilderResult out = {.slot_count = b->slot_count, .collider_count = b->colliders.len};
  out.slots = ALLOC_ARRAY(d->alloc, MeshBuilderSlotOut, b->slot_count);
  f32 uv = d->uv_per_metre > 0.0f ? d->uv_per_metre : 1.0f;
  for (u32 s = 0; s < b->slot_count; s++) {
    if (!b->used[s])
      continue;
    PgMesh *m = &b->slots[s];
    MeshBuilderSlotOut *so = &out.slots[s];
    so->bounds = pg_mesh_bounds(m);
    so->vertex_count = pg_mesh_vtx_count(m);
    so->tri_count = pg_mesh_tri_count(m);
    if (so->vertex_count == 0)
      continue;
    PgUpload up = pg_mesh_upload(d->renderer, m, uv);
    so->parts = ALLOC_ARRAY(d->alloc, HzHandleT(RendererSubMesh), up.count);
    for (u32 p = 0; p < up.count; p++)
      so->parts[p] = up.parts[p];
    so->part_count = up.count;
    out.vertex_count += so->vertex_count;
    out.part_count += up.count;
  }
  if (b->colliders.len > 0)
    mb_spawn_colliders(b, d);
  return out;
}
