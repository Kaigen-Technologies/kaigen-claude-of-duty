// Claude of Duty soldier bodies: the renderer objects (meshes, materials, models), the attach, the per-soldier pose system and the palette.

// ------------------------------------------------------------ anchors ---

// the weapon anchors in HandR bind-local space (Animator constructor)
hz_internal void cab_variant_anchors(const CabRig *rig, CabVariant *v) {
  quat q_inv = quat_conjugate(rig->bind_quat[COD_BONE_HAND_R]);
  v3 hand = rig->bind_pos[COD_BONE_HAND_R];
  const CabWeapon *w = &v->weapon;
  v->anchors.bore = v3_normalize(quat_rotate(q_inv, v3_normalize((v3){0.115f, -0.10f, 1.0f})));
  v->anchors.muzzle = quat_rotate(q_inv, v3_sub(w->muzzle, hand));
  v->anchors.foregrip = quat_rotate(q_inv, v3_sub(w->foregrip, hand));
  v->anchors.mag = quat_rotate(q_inv, v3_sub(w->mag_bottom, hand));
  v->anchors.eject = quat_rotate(q_inv, v3_sub(w->ejection, hand));
}

// ------------------------------------------------------------ materials ---

typedef struct CabMatDef {
  u32 set;        // CabSet, or CAB_SET_COUNT for none (glass)
  v3 tint;
  f32 rough;      // roughness scale
  f32 metal;      // metalness scale
  f32 normal;     // normal scale
  f32 ao;         // AO map strength
  u32 detail;     // CAB_BAKE_DETAIL_*, or 0 for none
  f32 detail_scale;
  f32 detail_normal;
  f32 detail_rough;
  f32 rim;        // rim strength scale
  f32 env;        // reflection scale
} CabMatDef;

#define CAB_NO_DETAIL 0xffffffffu
#define CAB_DETAIL_TILE 0.05f
#define CAB_RIM_STRENGTH 0.62f
#define CAB_RIM_EDGE 0.42f
#define CAB_RIM_POWER 1.9f

// for one variant and material slot
hz_internal CabMatDef cab_material_def(const CabVariantDef *V, u32 mat) {
  CabMatDef d = {.set = CAB_SET_COUNT, .tint = {1, 1, 1}, .rough = 1.0f, .metal = 1.0f, .normal = 1.0f, .ao = 0.85f, .detail = CAB_NO_DETAIL, .rim = 1.0f,
                 .env = 1.0f};
  switch (mat) {
  case CAB_MAT_CLOTH:
    d.set = V->camo;
    d.tint = V->cloth_tint;
    d.rough = 0.85f / 0.905f;
    d.normal = 1.15f;
    d.detail = CAB_BAKE_DETAIL_CLOTH;
    d.detail_scale = cab_mat_tile[CAB_MAT_CLOTH] / CAB_DETAIL_TILE;
    d.detail_normal = 0.45f;
    d.detail_rough = 0.16f;
    break;
  case CAB_MAT_PLATE:
    d.set = CAB_SET_PLATE;
    d.tint = V->plate_tint;
    d.rough = 0.55f / 0.62f;
    d.normal = 1.0f;
    d.detail = CAB_BAKE_DETAIL_NYLON;
    d.detail_scale = cab_mat_tile[CAB_MAT_PLATE] / CAB_DETAIL_TILE;
    d.detail_normal = 0.45f;
    d.detail_rough = 0.10f;
    break;
  case CAB_MAT_GEAR:
    d.set = CAB_SET_NYLON;
    d.tint = V->gear_tint;
    d.normal = 1.1f;
    d.detail = CAB_BAKE_DETAIL_NYLON;
    d.detail_scale = cab_mat_tile[CAB_MAT_GEAR] / CAB_DETAIL_TILE;
    d.detail_normal = 0.5f;
    d.detail_rough = 0.14f;
    break;
  case CAB_MAT_BOOT:
    d.set = CAB_SET_NYLON;
    d.tint = V->gear_tint;
    d.rough = 0.7f / 0.79f;
    d.normal = 1.1f;
    d.detail = CAB_BAKE_DETAIL_NYLON;
    d.detail_scale = cab_mat_tile[CAB_MAT_BOOT] / CAB_DETAIL_TILE;
    d.detail_normal = 0.5f;
    d.detail_rough = 0.10f;
    break;
  case CAB_MAT_SKIN:
    d.set = CAB_SET_SKIN;
    d.tint = V->skin_tint;
    d.normal = 0.8f;
    d.ao = 0.6f;
    break;
  case CAB_MAT_POLYMER: d.set = CAB_SET_POLYMER; break;
  case CAB_MAT_STEEL: d.set = CAB_SET_STEEL; break;
  case CAB_MAT_RUBBER:
    d.set = CAB_SET_RUBBER;
    d.normal = 1.2f;
    break;
  default: // the flat smoked glass
    d.tint = (v3){0.06f, 0.07f, 0.08f};
    d.rough = 0.11f;
    d.metal = 0.0f;
    d.ao = 1.0f;
    d.rim = 0.5f;
    d.env = 1.4f;
    break;
  }
  return d;
}

hz_internal HzHandleT(RendererMaterial) cab_make_material(RendererState *rs, const CabState *st, const CabVariant *v, u32 mat) {
  CabMatDef d = cab_material_def(v->def, mat);
  HzHandleT(RendererMaterial) m = renderer_create_material_instance(rs, st->base);
  material_set_vec4(rs, m, "tint", (v4){d.tint.x, d.tint.y, d.tint.z, 1.0f});
  material_set_vec4(rs, m, "pbr", (v4){d.rough, d.metal, d.normal, d.ao});
  material_set_vec4(rs, m, "rim_p", (v4){CAB_RIM_STRENGTH * d.rim, CAB_RIM_EDGE, CAB_RIM_POWER, 0.0f});
  material_set_vec4(rs, m, "misc", (v4){d.env, d.set < CAB_SET_COUNT ? 1.0f : 0.0f, 0.0f, 0.0f});
  if (d.set < CAB_SET_COUNT) {
    const CabBake *b = &st->tex->bake[d.set];
    material_set_texture(rs, m, "albedo", b->tex_albedo);
    material_set_texture(rs, m, "normal_map", b->tex_normal);
    material_set_texture(rs, m, "orm_map", b->tex_orm);
  }
  if (d.detail != CAB_NO_DETAIL) {
    material_set_vec4(rs, m, "detail_p", (v4){d.detail_scale, d.detail_normal, d.detail_rough, 1.0f});
    material_set_texture(rs, m, "detail_tex", st->tex->bake[d.detail].tex_albedo);
  }
  return m;
}

// one variant's geometry groups to the GPU, one submesh per material
hz_internal void cab_upload_variant(RendererState *rs, CabState *st, CabVariant *v) {
  for (u32 m = 0; m < CAB_MAT_COUNT; m++)
    v->mat[m] = cab_make_material(rs, st, v, m);
  tctx_temp_allocator_scope(ta, NULL) {
    for (u32 g = 0; g < v->group_count; g++) {
      const CabGroup *grp = &v->group[g];
      u16 *idx = ALLOC_ARRAY_NO_ZERO(&ta.allocator, u16, grp->icount);
      for (u32 i = 0; i < grp->icount; i++)
        idx[i] = (u16)(v->idx[grp->istart + i] - grp->vstart);
      aabb bb = aabb_invalidate();
      for (u32 i = 0; i < grp->vcount; i++)
        bb = aabb_encapsulate_point(bb, v->pos[grp->vstart + i]);
      SubMeshDesc d = {.vertex_count = grp->vcount,
                       .index_count = grp->icount,
                       .index_format = GPU_INDEX_FORMAT_U16,
                       .indices = idx,
                       .positions = v->pos + grp->vstart,
                       .normals = v->nrm + grp->vstart,
                       .uvs = v->uv + grp->vstart,
                       .colors = v->col + grp->vstart,
                       .colors_f16 = true,
                       .joint_indices = v->ji + grp->vstart * 4,
                       .joint_weights = v->jw + grp->vstart,
                       .bounds = bb};
      v->sub[g] = renderer_upload_submesh(rs, &d);
    }
  }
  v3 c = aabb_center(v->bounds);
  v3 h = v3_scale(aabb_half_extents(v->bounds), 1.45f);
  v->envelope = aabb_from_center_extents(c, h);
}

// main lane: the textures and the base material as they become ready, then the meshes and models
hz_internal void cab_render_stage(EcsWorld *w, CabState *st) {
  CodBodyProgress *prog = ecs_resource_get_mut(w, CodBodyProgress);
  if (!st->tex_uploaded) {
    cab_tex_upload(st->tex);
    st->tex_uploaded = true;
    st->progress = 0.7f;
    prog->progress = st->progress;
  }
  AssetSystem *assets = ecs_resource_get_mut(w, AssetSystem);
  if (!asset_is_ready(assets, st->base_asset))
    return;
  RendererState *rs = ecs_resource_get_mut(w, RendererState);
  st->base = renderer_create_material_from_asset(rs, assets, asset_get(assets, st->base_asset, MaterialBlobAsset));
  for (u32 i = 0; i < CAB_VARIANTS; i++)
    cab_upload_variant(rs, st, st->variant[i]);
  st->ready = true;
  st->stage = CAB_STAGE_DONE;
  st->progress = 1.0f;
  prog->progress = 1.0f;
  prog->ready = true;
}

// ---------------------------------------------------------------- systems ---

// the instance dies with its soldier (a match restart destroys the entities)
hz_internal void cab_anim_dtor(void *ptr, i32 count, const EcsTypeInfo *ti, EcsWorld *world) {
  UNUSED(ti);
  RendererState *r = ecs_resource_get_mut(world, RendererState);
  if (!r)
    return;
  CabAnim *a = (CabAnim *)ptr;
  // child 0 owns the palette the others borrow: it goes last
  for (i32 i = 0; i < count; i++)
    for (u32 c = a[i].child_count; c-- > 0;)
      renderer_submesh_destroy_instance(r, a[i].child[c]);
}

// one caller: instantiating mutates the renderer's shared instance tables
ecs_system(cab_attach_system, EcsIn(CodSoldier), EcsIn(CodSoldierDrive), EcsNot(CabAnim), EcsResIn(CodBodyState), EcsResInOut(RendererState)) {
  const CabState *st = ecs_sys_get_resource(CodBodyState)->s;
  if (!st->ready)
    return;
  RendererState *rs = ecs_sys_get_resource_mut(RendererState);
  for (i32 i = 0; i < it->count; i++) {
    const CodSoldier *s = ecs_sys_get_component(CodSoldier, i);
    const CodSoldierDrive *d = ecs_sys_get_component(CodSoldierDrive, i);
    u32 variant = s->variant < CAB_VARIANTS ? s->variant : 0;
    const CabVariant *v = st->variant[variant];
    CabAnim a;
    cab_anim_reset(&a, d, variant);
    // a soldier dead before its first pose still gets one: the ragdoll is built from it
    a.seen_death = 0;
    // the first submesh owns the palette, the rest skin off it
    u32 bone_offset = ~0u;
    aabb env = v->envelope;
    for (u32 g = 0; g < v->group_count; g++) {
      a.child[g] = renderer_submesh_instantiate(rs, &(RendererSubMeshInstanceDesc){.submesh = v->sub[g],
                                                                                   .material = v->mat[v->group[g].mat],
                                                                                   .transform = {.position = d->position, .rotation = quat_axis_angle((v3){0.0f, 1.0f, 0.0f}, d->yaw), .scale = {d->scale, d->scale, d->scale}},
                                                                                   .casts_shadow = true,
                                                                                   .local_bounds_override = &env,
                                                                                   .bone_count = g == 0 ? COD_BONE_COUNT : 0,
                                                                                   .bone_offset_borrow = g == 0 ? NULL : &bone_offset});
      if (g == 0)
        bone_offset = renderer_submesh_instance_cold(rs, a.child[0])->bone_offset;
    }
    a.child_count = v->group_count;
    ecs_component_set(it->world, it->entities[i], CabAnim, a);
  }
}

// the animator for every soldier: pose, palette, transform, and CodSoldierPose for the brain; parallel per entity
ecs_system(cab_pose_system, EcsIn(CodSoldierDrive), EcsInOut(CodSoldierPose), EcsInOut(CabAnim), EcsOptional(CabRagdoll), EcsResIn(CodBodyState),
           EcsResInOut(RendererState), EcsResIn(EcsPhysicsWorld)) {
  CabState *st = ecs_sys_get_resource(CodBodyState)->s;
  RendererState *rs = ecs_sys_get_resource_mut(RendererState);
  const EcsCollisionLayer *ground = ecs_physics_get_static_layer(it->world);
  tctx_temp_allocator_scope(ta, NULL) {
    m3x4 *palette = ALLOC_ARRAY_NO_ZERO(&ta.allocator, m3x4, COD_BONE_COUNT);
    for (i32 i = 0; i < it->count; i++) {
      const CodSoldierDrive *d = ecs_sys_get_component(CodSoldierDrive, i);
      CodSoldierPose *pose = ecs_sys_get_component_mut(CodSoldierPose, i);
      CabAnim *a = ecs_sys_get_component_mut(CabAnim, i);
      const CabRagdoll *rd = ecs_sys_get_component_optional(CabRagdoll, i);
      const CabVariant *v = st->variant[a->variant];
      const f32 scale = d->scale > 0.0f ? d->scale : 1.0f;
      CabProbe pc = {.layer = ground, .gpos = d->position, .grot = quat_axis_angle((v3){0.0f, 1.0f, 0.0f}, d->yaw), .scale = scale};

      if (rd && rd->active) {
        cab_ragdoll_pose(it->world, st, v, rd, a, d, pose, rs, palette);
        continue;
      }
      if (a->enabled) {
        cab_anim_state(a, d);
        a->accum += it->delta_time;
        b32 run = true;
        if (d->lod_irrelevant) {
          if (a->skip > 0) {
            a->skip--;
            run = false;
          } else {
            a->skip = 2;
          }
        } else {
          a->skip = 0;
        }
        if (run || !a->posed) {
          cab_anim_update(a, &st->rig, &v->anchors, d, a->accum, &pc);
          a->accum = 0.0f;
        }
        if (d->death_serial != a->seen_death) {
          a->seen_death = d->death_serial;
          a->want_ragdoll = true;
          a->enabled = false;
        }
      }
      if (!a->posed)
        continue;
      cab_palette(&st->rig, a->apos, a->arot, 1.0f, palette);
      renderer_submesh_instance_set_bones(rs, a->child[0], palette, COD_BONE_COUNT);
      Transform xf = {.position = d->position, .rotation = pc.grot, .scale = {scale, scale, scale}};
      cab_set_xform(rs, a, v, &xf);
      pose->ready = true;
      pose->reloading = a->reload_t >= 0.0f;
      pose->vaulting = a->vault_t >= 0.0f;
      pose->ragdoll = false;
      pose->muzzle = cab_to_world(&pc, a->muzzle);
      pose->eject = cab_to_world(&pc, a->eject);
      pose->muzzle_dir = quat_rotate(pc.grot, a->muzzle_dir);
      for (u32 b = 0; b < COD_BONE_COUNT; b++)
        pose->bones[b] = cab_to_world(&pc, a->apos[b]);
    }
  }
}

// ---------------------------------------------------------------- grounding ---

hz_internal void cab_ground_set(RendererState *rs, CabGroundSlot *g, u32 i, v3 pos, f32 yaw, f32 rx, f32 rz, b32 show) {
  Transform xf = {.position = (v3){pos.x, pos.y + 0.015f, pos.z}, .rotation = quat_axis_angle((v3){0.0f, 1.0f, 0.0f}, yaw), .scale = {rx * 2.0f, 1.0f, rz * 2.0f}};
  renderer_submesh_instance_update(rs, g->h[i], &(RendererSubmeshUpdateDesc){.transform = opt_some(Transform, xf), .visible = opt_some(b32, show)});
}

hz_internal const f32 cab_ground_build_power[2] = {3.4f, 4.6f};
hz_internal const f32 cab_ground_build_opacity[2] = {0.62f, 0.85f};
// the unit quad in the XZ plane, up-facing
hz_internal void cab_ground_build(RendererState *rs, CabState *st, const AssetSystem *assets) {
  st->gbase = renderer_create_material_from_asset(rs, assets, asset_get(assets, st->ground_asset, MaterialBlobAsset));
  for (u32 i = 0; i < 2; i++) {
    st->gmat[i] = renderer_create_material_instance(rs, st->gbase);
    material_set_vec4(rs, st->gmat[i], "p", (v4){cab_ground_build_power[i], cab_ground_build_opacity[i], 0.0f, 0.0f});
  }
  tctx_temp_allocator_scope(ta, NULL) {
    v3 *pos = ALLOC_ARRAY(&ta.allocator, v3, 4);
    v3 *nrm = ALLOC_ARRAY(&ta.allocator, v3, 4);
    v2 *uv = ALLOC_ARRAY(&ta.allocator, v2, 4);
    u16 *idx = ALLOC_ARRAY(&ta.allocator, u16, 6);
    for (u32 i = 0; i < 4; i++) {
      f32 x = (i & 1) ? 0.5f : -0.5f, z = (i & 2) ? 0.5f : -0.5f;
      pos[i] = (v3){x, 0.0f, z};
      nrm[i] = (v3){0.0f, 1.0f, 0.0f};
      uv[i] = (v2){x + 0.5f, z + 0.5f};
    }
    idx[0] = 0; idx[1] = 2; idx[2] = 1; idx[3] = 1; idx[4] = 2; idx[5] = 3;
    SubMeshDesc d = {.vertex_count = 4, .index_count = 6, .index_format = GPU_INDEX_FORMAT_U16, .indices = idx, .positions = pos, .normals = nrm, .uvs = uv,
                     .bounds = {{-0.5f, 0.0f, -0.5f}, {0.5f, 0.0f, 0.5f}}};
    st->gquad = renderer_upload_submesh(rs, &d);
  }
}

// main lane: the sprites follow every live soldier and are freed with it
hz_internal void cab_ground_frame(EcsWorld *w, CabState *st) {
  RendererState *rs = ecs_resource_get_mut(w, RendererState);
  if (!st->gtex_ready) {
    const AssetSystem *assets = ecs_resource_get(w, AssetSystem);
    if (!asset_is_ready(assets, st->ground_asset))
      return;
    cab_ground_build(rs, st, assets);
    st->gtex_ready = true;
  }
  for (u32 i = 0; i < CAB_GROUND_CAP; i++) {
    CabGroundSlot *g = &st->ground[i];
    if (g->used && !ecs_entity_exists(w, g->owner)) {
      for (u32 k = 0; k < 3; k++)
        renderer_submesh_destroy_instance(rs, g->h[k]);
      g->used = false;
    }
  }
  ecs_query_foreach(st->ground_query, qit) {
    const CodSoldierDrive *dr = ecs_iter_get_component(&qit, CodSoldierDrive, 0);
    const CodSoldierPose *po = ecs_iter_get_component(&qit, CodSoldierPose, 1);
    CabAnim *an = ecs_iter_get_component(&qit, CabAnim, 2);
    for (i32 i = 0; i < qit.count; i++) {
      if (an[i].ground_slot < 0) {
        for (u32 k = 0; k < CAB_GROUND_CAP; k++)
          if (!st->ground[k].used) {
            st->ground[k].used = true;
            st->ground[k].owner = qit.entities[i];
            an[i].ground_slot = (i32)k;
            for (u32 j = 0; j < 3; j++)
              st->ground[k].h[j] = renderer_submesh_instantiate(
                  rs, &(RendererSubMeshInstanceDesc){.submesh = st->gquad, .material = st->gmat[j == 0 ? 0 : 1], .sort_priority = 0});
            break;
          }
        if (an[i].ground_slot < 0)
          continue;
      }
      CabGroundSlot *g = &st->ground[an[i].ground_slot];
      const b32 live = po[i].ready && !po[i].ragdoll;
      const f32 s = dr[i].scale > 0.0f ? dr[i].scale : 1.0f;
      const f32 crouch = dr[i].crouch ? 0.86f : 1.0f;
      cab_ground_set(rs, g, 0, dr[i].position, dr[i].yaw, 0.44f * s * crouch, 0.34f * s * crouch, live);
      for (u32 f = 0; f < 2; f++) {
        v3 foot = po[i].bones[f == 0 ? COD_BONE_FOOT_R : COD_BONE_FOOT_L];
        f32 h = foot.y - dr[i].position.y;
        f32 k = 1.0f - m_minf(1.0f, m_maxf(0.0f, (h - 0.06f) / 0.29f));
        cab_ground_set(rs, g, 1 + f, (v3){foot.x, dr[i].position.y, foot.z}, dr[i].yaw, 0.15f * s * k, 0.21f * s * k, live && k > 0.05f);
      }
    }
  }
}
