// Claude of Duty weapons: build stages, materials, GPU upload, the viewmodel entities and their placement on the camera.

#define CWP_MAX_ENTS 640
#define CWP_MAX_MAG_MESH 12
#define CWP_RET_SEG 32
#define CWP_RET_ARCS 12
#define CWP_VIEW_FOV 60.0f
#define CWP_VIEW_DEPTH_RANGE 0.1f // reverse-Z band the viewmodel's depth is squeezed into
// display white in the world's exposure, cd/m2: how bright an emitter authored at 1.0 is drawn
#define CWP_EMIT_WHITE 18000.0f

typedef enum CwvKind {
  CWV_BODY = 0,
  CWV_PART,
  CWV_ARM,
  CWV_RETICLE,
} CwvKind;

// a viewmodel mesh entity: which gun, part, arm node or reticle layer it follows
typedef struct CodVmPart {
  u32 kind; // CwvKind
  u32 weapon;
  u32 part; // CwpPart, or the reticle layer
  u32 side; // arm: 0 right, 1 left
  u32 node; // arm node
  u32 mat;  // CwpMat
} CodVmPart;
ECS_COMPONENT_TYPEDEF(CodVmPart);

enum { CWP_BASE_LIT = 0, CWP_BASE_GLASS, CWP_BASE_UNLIT, CWP_BASE_ADD, CWP_BASE_COUNT };
enum { CWP_RET_HALO = 0, CWP_RET_RIM, CWP_RET_RING, CWP_RET_CORE, CWP_RET_COUNT };

struct CwpScene {
  HzHandleT(MaterialBlobAsset) base_asset[CWP_BASE_COUNT];
  HzHandleT(RendererMaterial) base[CWP_BASE_COUNT];
  u32 bases_ready;
  HzHandleT(RendererMaterial) mat[CWP_MAT_COUNT];
  HzHandleT(RendererMaterial) ret_mat[CWP_RET_COUNT];
  b32 mats_made;
  // upload cursor: guns first, then the arms, then the reticle
  u32 up_weapon;
  u32 up_arm;
  b32 up_reticle;
  EcsEntity ent[CWP_MAX_ENTS];
  b32 decal_done[CWP_MAX_ENTS];
  u32 ent_count;
  // the dropped magazine's meshes per gun
  HzHandleT(RendererSubMesh) mag_sub[COD_WEAPON_COUNT][CWP_MAX_MAG_MESH];
  u32 mag_mat[COD_WEAPON_COUNT][CWP_MAX_MAG_MESH];
  u32 mag_count[COD_WEAPON_COUNT];
  u32 tris;
  f32 ret_alpha_applied[CWP_RET_COUNT];
  // the reticle this frame, camera space
  b32 ret_show;
  v3 ret_pos;
  quat ret_rot;
  f32 ret_scale;
  f32 ret_alpha;
  u32 arm_pose[2];
  f32 progress;
};

hz_internal void cwp_attach_props(EcsWorld *w, CwpRig *r);

hz_internal const char *cwp_base_path(u32 i) {
  switch (i) {
  case CWP_BASE_LIT: return "claude_of_duty/weapons/cod_weapon_lit.hzt";
  case CWP_BASE_GLASS: return "claude_of_duty/weapons/cod_weapon_glass.hzt";
  case CWP_BASE_UNLIT: return "claude_of_duty/weapons/cod_weapon_unlit.hzt";
  default: return "claude_of_duty/weapons/cod_weapon_add.hzt";
  }
}

CwpScene *cwp_scene_make(EcsWorld *w, CwpRig *r) {
  CwpScene *s = ALLOC(&r->persist, CwpScene);
  AssetSystem *assets = ecs_resource_get_mut(w, AssetSystem);
  for (u32 i = 0; i < CWP_BASE_COUNT; i++)
    s->base_asset[i] = asset_load(MaterialBlobAsset, assets, .path = cwp_base_path(i));
  s->arm_pose[0] = s->arm_pose[1] = 0xffffffffu;
  return s;
}

// ---- build: every thread takes a whole gun or arm (a wide scope; each job has its own arena) ----

hz_internal void cwp_scene_build_wide(void *arg) {
  CwpRig *r = (CwpRig *)arg;
  Range(u32) rg = lane_range(5);
  for (u32 j = rg.min; j < rg.max; j++) {
    if (j < COD_WEAPON_COUNT)
      cwg_build_model(&r->models[j], j, r->arena[j]);
    else
      cwh_arm_build(&r->arm[j - COD_WEAPON_COUNT], j == COD_WEAPON_COUNT ? 1 : -1, j == COD_WEAPON_COUNT ? 1.0f : 0.97f, r->arena[j]);
  }
}

hz_internal void cwp_scene_build_lanes(CwpRig *r) { lane_scope_run(cwp_scene_build_wide, r); }

// ---- fit: the support hand on the handguard, contact AO on both sides, the clips ----

hz_internal const v3 cwp_default_left_finger = {0.82f, 0.5f, -0.28f};
hz_internal const v3 cwp_default_left_back = {-0.5f, 0.32f, -0.8f};

hz_internal b32 cwp_v3_zero(v3 a) { return a.x == 0.0f && a.y == 0.0f && a.z == 0.0f; }

hz_internal void cwp_scene_fit(CwpRig *r) {
  CwpArm *left = &r->arm[1];
  for (u32 i = 0; i < COD_WEAPON_COUNT; i++) {
    CwpModel *m = &r->models[i];
    const CwpDef *def = cwp_def(i);
    v3 lf = cwp_v3_zero(m->grip_l.finger) ? cwp_default_left_finger : m->grip_l.finger;
    v3 lb = cwp_v3_zero(m->grip_l.back) ? cwp_default_left_back : m->grip_l.back;
    if (m->handguard.valid && i != COD_WEAPON_PISTOL) {
      tctx_temp_allocator_scope(ta, NULL) {
        const u32 cap = 64;
        v3 *contacts = ALLOC_ARRAY_NO_ZERO(&ta.allocator, v3, cap);
        quat hq = cwp_hand_basis(lf, lb);
        cwh_arm_set_pose(left, CWH_POSE_CLAMP);
        u32 n = cwh_arm_fit_to_cylinder(left, m->grip_l.pos, hq, m->handguard.axis, m->handguard.dir, m->handguard.r, 0.001f,
                                        cwh_arm_fit_pose_slot(i), contacts, cap);
        f32 z0 = m_maxf(m->handguard.z0, m->handguard.z1);
        f32 z1 = m_minf(m->handguard.z0, m->handguard.z1);
        u32 kept = 0;
        for (u32 k = 0; k < n; k++)
          if (contacts[k].z <= z0 + 0.012f && contacts[k].z >= z1 - 0.012f)
            contacts[kept++] = contacts[k];
        cwh_arm_bake_contact_ao(left, contacts, kept, 0.012f, 0.7f);
        cwg_bake_contact_ao(m, contacts, kept, 0.012f, 0.9f);
      }
    }
    CwpClipNodes nodes = {.grip_pos = m->grip_l.pos,
                          .grip_finger = lf,
                          .grip_back = lb,
                          .mag_seat = m->mag_seat.pos,
                          .has_charge = m->has_part[CWP_PART_CHARGING],
                          .charge_rest = m->charge_rest.pos};
    cwp_build_clips(r->slot[i].clips, &nodes, def);
  }
  cwh_arm_set_pose(&r->arm[0], CWH_POSE_GRIP);
  cwh_arm_set_pose(left, CWH_POSE_FIT0 + 0);
}

// ---- materials ----

hz_internal v4 cwp_srgb4(u32 hex) {
  v3 s = {(f32)((hex >> 16) & 255u) / 255.0f, (f32)((hex >> 8) & 255u) / 255.0f, (f32)(hex & 255u) / 255.0f};
  v3 l = vec3_srgb_to_linear(s);
  return (v4){l.x, l.y, l.z, 1.0f};
}

hz_internal b32 cwp_valid(HzHandleT(RendererMaterial) h) { return handle_is_valid(HANDLE_CAST(HzHandle, h)); }

// the shared custom materials: base assets first, then one instance per use
hz_internal b32 cwp_scene_bases(EcsWorld *w, CwpScene *s) {
  AssetSystem *assets = ecs_resource_get_mut(w, AssetSystem);
  RendererState *rs = ecs_resource_get_mut(w, RendererState);
  for (u32 i = 0; i < CWP_BASE_COUNT; i++) {
    if (cwp_valid(s->base[i]) || !asset_is_ready(assets, s->base_asset[i]))
      continue;
    s->base[i] = renderer_create_material_from_asset(rs, assets, asset_get(assets, s->base_asset[i], MaterialBlobAsset));
    s->bases_ready++;
  }
  return s->bases_ready == CWP_BASE_COUNT;
}

hz_internal HzHandleT(RendererMaterial) cwp_emit_material(RendererState *rs, HzHandleT(RendererMaterial) base, v4 emission, f32 intensity) {
  HzHandleT(RendererMaterial) m = renderer_create_material_instance(rs, base);
  material_set_vec4(rs, m, "emission_color", emission);
  material_set_float(rs, m, "emission_intensity", intensity);
  return m;
}

hz_internal void cwp_scene_materials(EcsWorld *w, CwpScene *s) {
  RendererState *rs = ecs_resource_get_mut(w, RendererState);
  for (u32 k = 0; k <= CWP_MAT_SLEEVE; k++) {
    CodMaterialDesc d = cwp_material_desc(k);
    HzHandleT(RendererMaterial) m = cod_materials_get(w, &d);
    if (!cwp_valid(m))
      m = renderer_default_material(rs);
    else {
      const CwpMatDef *def = &cwp_mat_defs[k];
      material_set_vec4(rs, m, "wear_col", cwp_srgb4(def->wear_color));
      if (def->grime_color)
        material_set_vec4(rs, m, "grime_col", cwp_srgb4(def->grime_color));
      if (def->lib == COD_MAT_FABRIC)
        material_set_vec4(rs, m, "cloth_p", def->cloth);
    }
    s->mat[k] = m;
  }
  // the custom surfaces
  s->mat[CWP_MAT_CAVITY] = renderer_create_material_instance(rs, s->base[CWP_BASE_LIT]);
  material_set_vec4(rs, s->mat[CWP_MAT_CAVITY], "color", (v4){0.0030f, 0.0037f, 0.0044f, 1.0f});
  material_set_float(rs, s->mat[CWP_MAT_CAVITY], "smoothness", 0.0f);
  s->mat[CWP_MAT_OPTIC_TUBE] = renderer_create_material_instance(rs, s->base[CWP_BASE_LIT]);
  material_set_vec4(rs, s->mat[CWP_MAT_OPTIC_TUBE], "color", (v4){0.0123f, 0.0144f, 0.0168f, 1.0f});
  material_set_float(rs, s->mat[CWP_MAT_OPTIC_TUBE], "smoothness", 0.1f);
  s->mat[CWP_MAT_GLASS] = renderer_create_material_instance(rs, s->base[CWP_BASE_GLASS]);
  s->mat[CWP_MAT_LENS_RING] = cwp_emit_material(rs, s->base[CWP_BASE_ADD], cwp_srgb4(0x9fc4d8), 0.14f * CWP_EMIT_WHITE);
  material_set_vec4(rs, s->mat[CWP_MAT_LENS_RING], "color", (v4){0.0f, 0.0f, 0.0f, 0.5f});
  s->mat[CWP_MAT_LENS_VIG] = renderer_create_material_instance(rs, s->base[CWP_BASE_UNLIT]);
  // the reticle: halo, keyline, segmented ring, core
  s->ret_mat[CWP_RET_HALO] = cwp_emit_material(rs, s->base[CWP_BASE_ADD], cwp_srgb4(0xff2a0c), 0.34f * CWP_EMIT_WHITE);
  s->ret_mat[CWP_RET_RIM] = renderer_create_material_instance(rs, s->base[CWP_BASE_UNLIT]);
  material_set_vec4(rs, s->ret_mat[CWP_RET_RIM], "color", (v4){0.0070f, 0.0018f, 0.0030f, 0.0f});
  s->ret_mat[CWP_RET_RING] = cwp_emit_material(rs, s->base[CWP_BASE_ADD], cwp_srgb4(0xff1206), 0.95f * 0.5f * CWP_EMIT_WHITE);
  s->ret_mat[CWP_RET_CORE] = cwp_emit_material(rs, s->base[CWP_BASE_ADD], cwp_srgb4(0xff1206), 0.95f * CWP_EMIT_WHITE);
  for (u32 i = 0; i < CWP_RET_COUNT; i++)
    if (i != CWP_RET_RIM)
      material_set_vec4(rs, s->ret_mat[i], "color", (v4){0.0f, 0.0f, 0.0f, 0.0f});
  s->mats_made = true;
}

// ---- upload ----

typedef struct CwpGeo {
  v3 *pos;
  v3 *nrm;
  v2 *uv;
  v4 *col;
  u16 *idx;
  u32 nv;
  u32 ni;
} CwpGeo;

hz_internal HzHandleT(RendererSubMesh) cwp_upload_geo(RendererState *rs, const CwpGeo *g) {
  aabb bb = aabb_invalidate();
  for (u32 i = 0; i < g->nv; i++)
    bb = aabb_encapsulate_point(bb, g->pos[i]);
  SubMeshDesc d = {.vertex_count = g->nv,
                   .index_count = g->ni,
                   .index_format = GPU_INDEX_FORMAT_U16,
                   .indices = g->idx,
                   .positions = g->pos,
                   .normals = g->nrm,
                   .uvs = g->uv,
                   .colors = g->col,
                   .bounds = bb};
  return renderer_upload_submesh(rs, &d);
}

// an annulus in XY facing +Z over [a0, a0 + alen] with seg steps; alpha per vertex from `ramp` (inner at t=0)
hz_internal void cwp_ring_geo(Allocator *al, CwpGeo *g, f32 inner, f32 outer, u32 seg, f32 a0, f32 alen, v4 col_in, v4 col_out) {
  u32 vn = (seg + 1) * 2;
  g->nv = vn;
  g->ni = seg * 6;
  g->pos = ALLOC_ARRAY_NO_ZERO(al, v3, vn);
  g->nrm = ALLOC_ARRAY_NO_ZERO(al, v3, vn);
  g->uv = ALLOC_ARRAY(al, v2, vn);
  g->col = ALLOC_ARRAY_NO_ZERO(al, v4, vn);
  g->idx = ALLOC_ARRAY_NO_ZERO(al, u16, g->ni);
  for (u32 i = 0; i <= seg; i++) {
    f32 a = a0 + alen * (f32)i / (f32)seg;
    f32 c = m_cosf(a), sn = m_sinf(a);
    g->pos[i * 2] = (v3){c * inner, sn * inner, 0.0f};
    g->pos[i * 2 + 1] = (v3){c * outer, sn * outer, 0.0f};
    g->nrm[i * 2] = g->nrm[i * 2 + 1] = (v3){0.0f, 0.0f, 1.0f};
    g->col[i * 2] = col_in;
    g->col[i * 2 + 1] = col_out;
  }
  for (u32 i = 0; i < seg; i++) {
    u16 a = (u16)(i * 2), b = (u16)(i * 2 + 1), c = (u16)(i * 2 + 2), d = (u16)(i * 2 + 3);
    u16 *t = &g->idx[i * 6];
    t[0] = a; t[1] = b; t[2] = c;
    t[3] = b; t[4] = d; t[5] = c;
  }
}

// registers a spawned entity for the decal fixup
hz_internal void cwp_ent_track(CwpScene *s, EcsEntity e) {
  if (s->ent_count < CWP_MAX_ENTS)
    s->ent[s->ent_count++] = e;
}

hz_internal EcsEntity cwp_spawn_mesh(EcsWorld *w, RendererState *rs, CwpScene *s, HzHandleT(RendererSubMesh) sub, u32 mat, i16 priority, CodVmPart tag) {
  EcsEntity e = ecs_renderer_spawn(w, rs,
                                   &(EcsRendererSpawnDesc){.submesh = sub,
                                                           .material = s->mat[mat],
                                                           .transform = {.position = V3_ZERO, .rotation = quat_identity(), .scale = V3_ONE},
                                                           .casts_shadow = false,
                                                           .hidden = true,
                                                           .sort_priority = priority});
  ecs_component_set(w, e, CodVmPart, tag);
  ecs_component_set(w, e, EcsProjectionOverride, {0});
  cwp_ent_track(s, e);
  return e;
}

hz_internal i16 cwp_mat_priority(u32 mat) {
  switch (mat) {
  case CWP_MAT_GLASS: return 4;
  case CWP_MAT_LENS_VIG: return 5;
  case CWP_MAT_LENS_RING: return 6;
  default: return 0;
  }
}

// the vignette: a disc whose alpha ramps from clear to opaque over the outer fifth of the aperture
hz_internal HzHandleT(RendererSubMesh) cwp_upload_vignette(RendererState *rs, const PgMesh *m) {
  aabb bb = pg_mesh_bounds(m);
  v3 c = aabb_center(bb);
  f32 radius = (bb.max.x - bb.min.x) * 0.5f;
  HzHandleT(RendererSubMesh) h = {0};
  tctx_temp_allocator_scope(ta, NULL) {
    Allocator *al = &ta.allocator;
    const u32 rings = 8, seg = 32;
    CwpGeo g = {.nv = (rings + 1) * seg + 1, .ni = (rings * seg * 6) + seg * 3};
    g.pos = ALLOC_ARRAY_NO_ZERO(al, v3, g.nv);
    g.nrm = ALLOC_ARRAY_NO_ZERO(al, v3, g.nv);
    g.uv = ALLOC_ARRAY(al, v2, g.nv);
    g.col = ALLOC_ARRAY_NO_ZERO(al, v4, g.nv);
    g.idx = ALLOC_ARRAY_NO_ZERO(al, u16, g.ni);
    g.pos[0] = c;
    g.nrm[0] = (v3){0.0f, 0.0f, 1.0f};
    g.col[0] = (v4){1.0f, 1.0f, 1.0f, 0.0f};
    for (u32 ri = 0; ri <= rings; ri++) {
      // rings are packed toward the rim where the ramp lives: 0.8..1.0 in steps, the rest one flat band
      f32 rr = ri == 0 ? 0.8f : 0.8f + 0.2f * (f32)ri / (f32)rings;
      f32 t = m_clampf((rr - 0.8f) / 0.2f, 0.0f, 1.0f);
      f32 a = t * t * (3.0f - 2.0f * t);
      for (u32 k = 0; k < seg; k++) {
        f32 an = (f32)(2.0 * M_PI) * (f32)k / (f32)seg;
        u32 i = 1 + ri * seg + k;
        g.pos[i] = (v3){c.x + m_cosf(an) * radius * rr, c.y + m_sinf(an) * radius * rr, c.z};
        g.nrm[i] = (v3){0.0f, 0.0f, 1.0f};
        g.col[i] = (v4){1.0f, 1.0f, 1.0f, a};
      }
    }
    u32 ti = 0;
    for (u32 k = 0; k < seg; k++) {
      g.idx[ti++] = 0;
      g.idx[ti++] = (u16)(1 + k);
      g.idx[ti++] = (u16)(1 + (k + 1) % seg);
    }
    for (u32 ri = 0; ri < rings; ri++) {
      for (u32 k = 0; k < seg; k++) {
        u16 a = (u16)(1 + ri * seg + k), b = (u16)(1 + ri * seg + (k + 1) % seg);
        u16 c2 = (u16)(1 + (ri + 1) * seg + k), d = (u16)(1 + (ri + 1) * seg + (k + 1) % seg);
        g.idx[ti++] = a; g.idx[ti++] = c2; g.idx[ti++] = b;
        g.idx[ti++] = b; g.idx[ti++] = c2; g.idx[ti++] = d;
      }
    }
    g.ni = ti;
    h = cwp_upload_geo(rs, &g);
  }
  return h;
}

hz_internal void cwp_upload_asm(EcsWorld *w, RendererState *rs, CwpScene *s, u32 weapon, u32 kind, u32 part, CwpAsm *a) {
  for (u32 mat = 0; mat < CWP_MAT_COUNT; mat++) {
    if (!a->used[mat] || a->mesh[mat].pos.len == 0)
      continue;
    PgMesh *pm = &a->mesh[mat];
    s->tris += pg_mesh_tri_count(pm);
    if (mat == CWP_MAT_LENS_VIG) {
      HzHandleT(RendererSubMesh) h = cwp_upload_vignette(rs, pm);
      cwp_spawn_mesh(w, rs, s, h, mat, cwp_mat_priority(mat), (CodVmPart){.kind = kind, .weapon = weapon, .part = part, .mat = mat});
      continue;
    }
    PgUpload up = pg_mesh_upload(rs, pm, 1.0f);
    for (u32 i = 0; i < up.count; i++) {
      cwp_spawn_mesh(w, rs, s, up.parts[i], mat, cwp_mat_priority(mat), (CodVmPart){.kind = kind, .weapon = weapon, .part = part, .mat = mat});
      if (kind == CWV_PART && part == CWP_PART_MAGAZINE && s->mag_count[weapon] < CWP_MAX_MAG_MESH) {
        s->mag_sub[weapon][s->mag_count[weapon]] = up.parts[i];
        s->mag_mat[weapon][s->mag_count[weapon]++] = mat;
      }
    }
  }
}

hz_internal void cwp_upload_weapon(EcsWorld *w, RendererState *rs, CwpScene *s, CwpRig *r, u32 i) {
  CwpModel *m = &r->models[i];
  cwp_upload_asm(w, rs, s, i, CWV_BODY, 0, &m->body);
  for (u32 p = 0; p < CWP_PART_COUNT; p++)
    if (m->has_part[p])
      cwp_upload_asm(w, rs, s, i, CWV_PART, p, &m->part[p]);
}

hz_internal void cwp_upload_arm(EcsWorld *w, RendererState *rs, CwpScene *s, CwpRig *r, u32 side) {
  CwpArm *a = &r->arm[side];
  for (u32 i = 0; i < a->mesh_count; i++) {
    CwpArmMesh *am = &a->mesh[i];
    if (am->mesh.pos.len == 0)
      continue;
    s->tris += pg_mesh_tri_count(&am->mesh);
    PgUpload up = pg_mesh_upload(rs, &am->mesh, 1.0f);
    for (u32 k = 0; k < up.count; k++)
      cwp_spawn_mesh(w, rs, s, up.parts[k], am->mat, 0, (CodVmPart){.kind = CWV_ARM, .side = side, .node = am->node, .mat = am->mat});
  }
}

hz_internal void cwp_upload_reticle(EcsWorld *w, RendererState *rs, CwpScene *s) {
  tctx_temp_allocator_scope(ta, NULL) {
    Allocator *al = &ta.allocator;
    CwpGeo g;
    v4 white = {1.0f, 1.0f, 1.0f, 1.0f};
    const f32 tau = (f32)(2.0 * M_PI);
    HzHandleT(RendererSubMesh) sub[CWP_RET_COUNT];
    // a disc is an annulus from the centre out: a thin inner radius keeps the one builder
    cwp_ring_geo(al, &g, 0.0f, 1.6f, CWP_RET_SEG, 0.0f, tau, white, white);
    sub[CWP_RET_HALO] = cwp_upload_geo(rs, &g);
    cwp_ring_geo(al, &g, 1.0f, 1.42f, CWP_RET_SEG, 0.0f, tau, white, white);
    sub[CWP_RET_RIM] = cwp_upload_geo(rs, &g);
    cwp_ring_geo(al, &g, 0.0f, 1.0f, CWP_RET_SEG, 0.0f, tau, white, white);
    sub[CWP_RET_CORE] = cwp_upload_geo(rs, &g);
    // the segmented ring: twelve arcs of 56% of a twelfth, 4 steps each, in one mesh
    {
      CwpGeo one;
      u32 per = 5 * 2;
      g.nv = per * CWP_RET_ARCS;
      g.ni = 4 * 6 * CWP_RET_ARCS;
      g.pos = ALLOC_ARRAY_NO_ZERO(al, v3, g.nv);
      g.nrm = ALLOC_ARRAY_NO_ZERO(al, v3, g.nv);
      g.uv = ALLOC_ARRAY(al, v2, g.nv);
      g.col = ALLOC_ARRAY_NO_ZERO(al, v4, g.nv);
      g.idx = ALLOC_ARRAY_NO_ZERO(al, u16, g.ni);
      for (u32 k = 0; k < CWP_RET_ARCS; k++) {
        f32 a0 = tau * (f32)k / (f32)CWP_RET_ARCS;
        cwp_ring_geo(al, &one, 2.98f, 3.42f, 4, a0, (tau / (f32)CWP_RET_ARCS) * 0.56f, white, white);
        for (u32 i = 0; i < one.nv; i++) {
          g.pos[k * per + i] = one.pos[i];
          g.nrm[k * per + i] = one.nrm[i];
          g.col[k * per + i] = one.col[i];
        }
        for (u32 i = 0; i < one.ni; i++)
          g.idx[k * 24 + i] = (u16)(one.idx[i] + k * per);
      }
      sub[CWP_RET_RING] = cwp_upload_geo(rs, &g);
    }
    static const i16 order[CWP_RET_COUNT] = {19, 20, 20, 21};
    for (u32 i = 0; i < CWP_RET_COUNT; i++) {
      EcsEntity e = ecs_renderer_spawn(w, rs,
                                       &(EcsRendererSpawnDesc){.submesh = sub[i],
                                                               .material = s->ret_mat[i],
                                                               .transform = {.position = V3_ZERO, .rotation = quat_identity(), .scale = V3_ONE},
                                                               .hidden = true,
                                                               .sort_priority = order[i]});
      ecs_component_set(w, e, CodVmPart, ((CodVmPart){.kind = CWV_RETICLE, .part = i}));
      ecs_component_set(w, e, EcsProjectionOverride, {0});
      cwp_ent_track(s, e);
    }
  }
}

// one slice of the GPU upload within the frame's budget; true when everything is spawned
hz_internal b32 cwp_scene_upload(EcsWorld *w, CwpRig *r) {
  CwpScene *s = r->scene;
  RendererState *rs = ecs_resource_get_mut(w, RendererState);
  u64 t0 = os_time_now();
  const f64 budget_ms = 4.0;
  while (s->up_weapon < COD_WEAPON_COUNT) {
    cwp_upload_weapon(w, rs, s, r, s->up_weapon++);
    s->progress = 0.7f + 0.3f * (f32)s->up_weapon / 6.0f;
    if (os_ticks_to_ms(os_time_diff(os_time_now(), t0)) > budget_ms)
      return false;
  }
  while (s->up_arm < 2) {
    cwp_upload_arm(w, rs, s, r, s->up_arm++);
    s->progress = 0.7f + 0.3f * (f32)(COD_WEAPON_COUNT + s->up_arm) / 6.0f;
    if (os_ticks_to_ms(os_time_diff(os_time_now(), t0)) > budget_ms)
      return false;
  }
  if (!s->up_reticle) {
    cwp_upload_reticle(w, rs, s);
    s->up_reticle = true;
  }
  return true;
}

// ---- the arms and the reticle, from the rig's outputs (single thread) ----

hz_internal void cwp_scene_apply_arms(CwpRig *r, const CwpModel *model) {
  CwpScene *s = r->scene;
  CwpView *v = &r->view;
  u32 right_pose = CWH_POSE_GRIP;
  u32 left_pose = v->hand_pose_l >= CWP_POSE_FITTED ? cwh_arm_fit_pose_slot(v->hand_pose_l - CWP_POSE_FITTED) : v->hand_pose_l;
  CwpArm *ar = &r->arm[0];
  CwpArm *al = &r->arm[1];
  if (s->arm_pose[0] != right_pose) {
    cwh_arm_set_pose(ar, right_pose);
    s->arm_pose[0] = right_pose;
  }
  if (s->arm_pose[1] != left_pose) {
    cwh_arm_set_pose(al, left_pose);
    s->arm_pose[1] = left_pose;
  }
  cwh_arm_set_trigger(ar, v->trigger_out);
  cwh_arm_solve(ar, v->shoulder_r, v->hand_pos_r, v->hand_rot_r);
  cwh_arm_solve(al, v->shoulder_l, v->hand_pos_l, v->hand_rot_l);

  // the collimated dot: the tube axis from the eye, independent of where the eye is
  s->ret_show = false;
  if (!model->optic.valid)
    return;
  v3 center = v3_add(quat_rotate(v->rig_rot, model->optic.center), v->rig_pos);
  v3 axis = v3_normalize(quat_rotate(v->rig_rot, (v3){0.0f, 0.0f, -1.0f}));
  f32 dist = v3_dot(center, axis);
  if (dist <= 0.02f)
    return;
  v3 dot = v3_scale(axis, dist);
  v3 off = v3_sub(dot, center);
  f32 off_len = m_sqrtf(off.x * off.x + off.y * off.y);
  f32 aperture = model->optic.aperture_r > 0.0f ? model->optic.aperture_r : 0.01f;
  f32 ads = cwp_smootherstep(0.0f, 1.0f, v->ads_t);
  f32 alpha = (1.0f - cwp_smootherstep(aperture * 0.5f, aperture * 1.05f, off_len)) * m_lerpf(0.55f, 1.0f, ads);
  if (alpha <= 0.01f)
    return;
  v3 z = v3_normalize(v3_negate(dot));
  v3 x = v3_normalize(v3_cross((v3){0.0f, 1.0f, 0.0f}, z));
  v3 y = v3_cross(z, x);
  s->ret_rot = quat_from_m3((m3){{{x.x, x.y, x.z}, {y.x, y.y, y.z}, {z.x, z.y, z.z}}});
  s->ret_pos = dot;
  s->ret_scale = dist * m_lerpf(0.00385f, 0.00655f, ads);
  s->ret_alpha = alpha;
  s->ret_show = true;}

// ---- placement: every viewmodel mesh follows its node on the final camera ----

hz_internal void cwp_part_local(const CwpRig *r, const CodVmPart *p, v3 *pos, quat *rot, b32 *visible) {
  const CwpView *v = &r->view;
  const CwpModel *m = &r->models[p->weapon];
  *pos = V3_ZERO;
  *rot = quat_identity();
  *visible = true;
  if (p->kind == CWV_BODY)
    return;
  switch (p->part) {
  case CWP_PART_MAGAZINE:
    *pos = v->mag_pos;
    *rot = v->mag_rot;
    *visible = v->mag_visible;
    break;
  case CWP_PART_CHARGING:
    *pos = v->charge_pos;
    *rot = cwp_quat_euler_xyz(m->charge_rest.rot);
    break;
  case CWP_PART_BOLT:
    *pos = v->bolt_pos;
    *rot = cwp_quat_euler_xyz(m->bolt_rest.rot);
    break;
  case CWP_PART_SLIDE:
    *pos = v->slide_pos;
    *rot = cwp_quat_euler_xyz(m->slide_rest.rot);
    break;
  case CWP_PART_TRIGGER:
    *pos = m->trigger_pivot.pos;
    *rot = cwp_quat_euler_xyz((v3){v->trigger_rot, m->trigger_pivot.rot.y, m->trigger_pivot.rot.z});
    break;
  default:
    *pos = m->selector_pivot.pos;
    *rot = cwp_quat_euler_xyz((v3){0.0f, m->selector_pivot.rot.y, m->selector_pivot.rot.z});
    break;
  }
}

ecs_system(cwp_place_system, EcsIn(CodVmPart), EcsInOut(LocalTransform), EcsInOut(WorldTransform), EcsInOut(EcsVisibility),
           EcsResIn(CodWeaponsRig), EcsResIn(CodPlayerState), EcsResIn(CodWeaponState)) {
  const CwpRig *r = ecs_sys_get_resource(CodWeaponsRig)->rig;
  const CodPlayerState *ps = ecs_sys_get_resource(CodPlayerState);
  const CodWeaponState *ws = ecs_sys_get_resource(CodWeaponState);
  const CwpScene *s = r->scene;
  b32 show = r->ready && ws->visible && ps->ready && !ps->dead;
  for (i32 i = 0; i < it->count; i++) {
    const CodVmPart *p = ecs_sys_get_component(CodVmPart, i);
    b32 visible = show;
    v3 lp = V3_ZERO;
    quat lq = quat_identity();
    v3 scale = V3_ONE;
    switch (p->kind) {
    case CWV_BODY:
    case CWV_PART: {
      b32 part_visible;
      cwp_part_local(r, p, &lp, &lq, &part_visible);
      visible = visible && p->weapon == r->active && part_visible;
      break;
    }
    case CWV_ARM:
      cwh_arm_node_world(&r->arm[p->side], p->node, &lp, &lq);
      break;
    default:
      visible = visible && s->ret_show;
      if (visible) {
        // reticle layers are authored in the camera frame: back into the rig frame is not needed, they go through the camera directly
        lp = s->ret_pos;
        lq = s->ret_rot;
        scale = (v3){s->ret_scale, s->ret_scale, s->ret_scale};
      }
      break;
    }
    EcsVisibility *vis = ecs_sys_get_component_mut(EcsVisibility, i);
    if (vis->visible != visible)
      vis->visible = visible;
    if (!visible)
      continue;
    v3 cam_local_pos;
    quat cam_local_rot;
    if (p->kind == CWV_RETICLE) {
      cam_local_pos = lp;
      cam_local_rot = lq;
    } else {
      cam_local_pos = v3_add(r->view.rig_pos, quat_rotate(r->view.rig_rot, lp));
      cam_local_rot = quat_mul(r->view.rig_rot, lq);
    }
    v3 wp = v3_add(ps->eye, quat_rotate(ps->view_rot, cam_local_pos));
    quat wq = quat_mul(ps->view_rot, cam_local_rot);
    LocalTransform *lt = ecs_sys_get_component_mut(LocalTransform, i);
    WorldTransform *wt = ecs_sys_get_component_mut(WorldTransform, i);
    lt->position = wt->position = wp;
    lt->rotation = wt->rotation = wq;
    lt->scale = wt->scale = scale;
  }
}

// the load progress of the weapons stage
ecs_system(cwp_load_system, EcsResIn(CodWeaponsRig), EcsResInOut(CodLoad)) {
  const CwpRig *r = ecs_sys_get_resource(CodWeaponsRig)->rig;
  ecs_sys_get_resource_mut(CodLoad)->progress[COD_LOAD_WEAPONS] = r->ready ? 1.0f : r->scene->progress;
}

// ---- the main-lane frame: stages, the projection override, decal layers, reticle opacity ----

hz_internal void cwp_scene_frame(EcsWorld *w, CwpRig *r) {
  CwpScene *s = r->scene;
  RendererState *rs = ecs_resource_get_mut(w, RendererState);
  switch (r->stage) {
  case CWP_STAGE_BUILD:
    // the lanes finished their jobs before the barrier that closed the previous frame
    if (r->models[COD_WEAPON_PISTOL].tris > 0 && r->arm[1].mesh_count > 0) {
      r->build_ms = os_ticks_to_ms(os_time_diff(os_time_now(), (u64)r->stage_t0));
      r->stage = CWP_STAGE_FIT;
      s->progress = 0.4f;
    }
    break;
  case CWP_STAGE_FIT: {
    u64 t0 = os_time_now();
    cwp_scene_fit(r);
    log_info("weapons: built % tris in % ms, fitted in % ms", fmt_u32(r->models[0].tris + r->models[1].tris + r->models[2].tris),
             fmt_f64_p(r->build_ms, 0), fmt_f64_p(os_ticks_to_ms(os_time_diff(os_time_now(), t0)), 1));
    r->stage = CWP_STAGE_UPLOAD;
    s->progress = 0.6f;
    break;
  }
  case CWP_STAGE_UPLOAD:
    if (!s->mats_made) {
      if (!ecs_resource_get(w, CodMaterials)->ready || !cwp_scene_bases(w, s))
        break;
      cwp_scene_materials(w, s);
      s->progress = 0.7f;
    }
    if (cwp_scene_upload(w, r)) {
      r->stage = CWP_STAGE_DONE;
      r->ready = true;
      s->progress = 1.0f;
      cwp_view_play(&r->view, CWP_CLIP_DRAW);
      log_info("weapons: % tris on the GPU in % entities", fmt_u32(s->tris), fmt_u32(s->ent_count));
    }
    break;
  default:
    break;
  }
  if (!r->ready)
    return;
  cwp_attach_props(w, r);

  // the viewmodel's own projection: 60 degrees at hip, narrower as the sight comes up
  CwpView *v = &r->view;
  renderer_set_projection_override(rs, &(RendererProjectionOverrideDesc){.fov_degrees = CWP_VIEW_FOV * v->fov_scale, .depth_range = CWP_VIEW_DEPTH_RANGE});

  // world decals never land on the gun
  for (u32 i = 0; i < s->ent_count; i++) {
    if (s->decal_done[i])
      continue;
    const EcsSubMeshInstance *si = ecs_component_get(w, s->ent[i], EcsSubMeshInstance);
    if (si && handle_is_valid(HANDLE_CAST(HzHandle, si->instance))) {
      renderer_submesh_instance_set_decal_layers(rs, si->instance, DECAL_LAYERS_NONE);
      s->decal_done[i] = true;
    }
  }

  // the dot's opacity follows the vignette
  f32 a = s->ret_show ? s->ret_alpha : 0.0f;
  f32 want[CWP_RET_COUNT] = {a * 0.06f, a * 0.8f, a, a};
  for (u32 i = 0; i < CWP_RET_COUNT; i++) {
    if (m_absf(s->ret_alpha_applied[i] - want[i]) < 1e-3f)
      continue;
    s->ret_alpha_applied[i] = want[i];
    if (i == CWP_RET_RIM)
      material_set_vec4(rs, s->ret_mat[i], "color", (v4){0.0070f, 0.0018f, 0.0030f, want[i]});
    else
      material_set_vec4(rs, s->ret_mat[i], "color", (v4){0.0f, 0.0f, 0.0f, want[i]});
  }
}
