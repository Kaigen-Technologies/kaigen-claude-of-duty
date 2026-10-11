// Claude of Duty soldier bodies: the merged geometry, skin weights and baked vertex shading.

// the occlusion proxies, in build order
void cab_occluders(CabOcc *out, u32 *count) {
  u32 n = 0;
#define OCC(a, b, r, k) out[n++] = (CabOcc){a, b, r, k}
  const v3 shR = cod_rig_bind[COD_BONE_UPPER_ARM_R], elR = cod_rig_bind[COD_BONE_FOREARM_R], wrR = cod_rig_bind[COD_BONE_HAND_R];
  const v3 shL = cod_rig_bind[COD_BONE_UPPER_ARM_L], elL = cod_rig_bind[COD_BONE_FOREARM_L], wrL = cod_rig_bind[COD_BONE_HAND_L];
  const v3 hipR = cod_rig_bind[COD_BONE_UP_LEG_R], knR = cod_rig_bind[COD_BONE_LEG_R], anR = cod_rig_bind[COD_BONE_FOOT_R];
  const v3 hipL = cod_rig_bind[COD_BONE_UP_LEG_L], knL = cod_rig_bind[COD_BONE_LEG_L], anL = cod_rig_bind[COD_BONE_FOOT_L];
  const v3 bore = v3_normalize((v3){0.115f, -0.10f, 1.0f});
  const v3 grip = cod_rig_bind[COD_BONE_HAND_R];
  OCC(((v3){0, 0.95f, -0.01f}), ((v3){0, 1.42f, 0.0f}), 0.155f, 1.0f);
  OCC(((v3){0, 1.17f, 0.130f}), ((v3){0, 1.40f, 0.138f}), 0.11f, 1.0f);
  OCC(((v3){0, 1.20f, -0.105f}), ((v3){0, 1.40f, -0.112f}), 0.105f, 0.8f);
  OCC(((v3){-0.17f, 1.16f, 0.02f}), ((v3){-0.17f, 1.30f, 0.02f}), 0.055f, 0.7f);
  OCC(((v3){0.17f, 1.16f, 0.02f}), ((v3){0.17f, 1.30f, 0.02f}), 0.055f, 0.7f);
  OCC(((v3){0, 1.60f, 0.0f}), ((v3){0, 1.80f, -0.01f}), 0.122f, 1.0f);
  OCC(((v3){-0.09f, 1.655f, 0.05f}), ((v3){0.09f, 1.655f, 0.05f}), 0.055f, 1.0f);
  OCC(shR, elR, 0.058f, 0.7f);
  OCC(shL, elL, 0.058f, 0.7f);
  OCC(hipR, knR, 0.085f, 0.8f);
  OCC(hipL, knL, 0.085f, 0.8f);
  OCC(((v3){0, 0.90f, -0.01f}), ((v3){0, 1.05f, -0.01f}), 0.15f, 0.8f);
  OCC(((v3){-0.085f, 1.42f, 0.06f}), ((v3){-0.085f, 1.42f, -0.06f}), 0.048f, 1.0f);
  OCC(((v3){0.085f, 1.42f, 0.06f}), ((v3){0.085f, 1.42f, -0.06f}), 0.048f, 1.0f);
  OCC(((v3){-0.13f, 1.40f, 0.055f}), ((v3){0.10f, 1.10f, 0.115f}), 0.032f, 0.9f);
  OCC(((v3){-0.02f, 1.02f, 0.10f}), ((v3){-0.02f, 1.02f, -0.10f}), 0.10f, 0.6f);
  OCC(wrR, ((v3){wrR.x, wrR.y + 0.05f, wrR.z}), 0.044f, 0.9f);
  OCC(wrL, ((v3){wrL.x, wrL.y + 0.05f, wrL.z}), 0.044f, 0.9f);
  OCC(elR, ((v3){elR.x, elR.y - 0.02f, elR.z - 0.01f}), 0.052f, 0.8f);
  OCC(elL, ((v3){elL.x, elL.y - 0.02f, elL.z - 0.01f}), 0.052f, 0.8f);
  OCC(knR, ((v3){knR.x, knR.y - 0.03f, knR.z + 0.01f}), 0.072f, 0.8f);
  OCC(knL, ((v3){knL.x, knL.y - 0.03f, knL.z + 0.01f}), 0.072f, 0.8f);
  OCC(((v3){anR.x, anR.y + 0.09f, anR.z}), ((v3){anR.x, anR.y + 0.15f, anR.z}), 0.062f, 0.8f);
  OCC(((v3){anL.x, anL.y + 0.09f, anL.z}), ((v3){anL.x, anL.y + 0.15f, anL.z}), 0.062f, 0.8f);
  OCC(grip, ((v3){grip.x + bore.x * 0.4f, grip.y + bore.y * 0.4f + 0.09f, grip.z + bore.z * 0.4f}), 0.045f, 0.6f);
  for (u32 i = 0; i < 3; i++) {
    f32 x = ((f32)i - 1.0f) * 0.078f;
    OCC(((v3){x, 1.20f, 0.160f}), ((v3){x, 1.28f, 0.164f}), 0.040f, 0.8f);
  }
#undef OCC
  *count = n;
}

// a variant's vertex and index offsets in material-grouped order, and its arrays; one lane
hz_internal void cab_variant_layout(Allocator *alloc, CabVariant *v) {
  u32 vo = 0, io = 0;
  v->group_count = 0;
  u32 cur = 0xffffffffu;
  for (u32 k = 0; k < v->order_count; k++) {
    CabSpec *s = &v->spec[v->order[k]];
    u32 n = s->mesh ? (u32)s->mesh->pos.len : 0;
    u32 ni = s->mesh ? (u32)s->mesh->idx.len : 0;
    if (s->mat != cur) {
      cur = s->mat;
      CabGroup *g = &v->group[v->group_count++];
      *g = (CabGroup){.mat = cur, .vstart = vo, .istart = io};
    }
    s->vo = vo;
    s->vn = n;
    s->io = io;
    s->in = ni;
    vo += n;
    io += ni;
    CabGroup *g = &v->group[v->group_count - 1];
    g->vcount = vo - g->vstart;
    g->icount = io - g->istart;
  }
  v->vertex_count = vo;
  v->index_count = io;
  v->pos = ALLOC_ARRAY_NO_ZERO(alloc, v3, vo);
  v->nrm = ALLOC_ARRAY_NO_ZERO(alloc, v3, vo);
  v->uv = ALLOC_ARRAY_NO_ZERO(alloc, v2, vo);
  v->col = ALLOC_ARRAY_NO_ZERO(alloc, v4, vo);
  v->ji = ALLOC_ARRAY(alloc, u16, vo * 4);
  v->jw = ALLOC_ARRAY(alloc, v4, vo);
  v->idx = ALLOC_ARRAY_NO_ZERO(alloc, u32, io);
}

hz_internal f32 cab_seg_dist(v3 p, v3 a, v3 b, v3 *closest) {
  v3 d = v3_sub(b, a);
  f32 l2 = v3_lengthsq(d);
  f32 t = l2 > 1e-12f ? v3_dot(v3_sub(p, a), d) / l2 : 0.0f;
  t = m_clampf(t, 0.0f, 1.0f);
  *closest = v3_add(a, v3_scale(d, t));
  return v3_length(v3_sub(p, *closest));
}

// the working set of the four strongest candidates
typedef struct CabPick {
  i32 id[4];
  f32 w[4];
  f32 out[4];
} CabPick;

// the four strongest candidate bones of a vertex, weights normalised
hz_internal void cab_bind_vertex(const CabRig *rig, const CabSpec *s, v3 p, u16 *ji, v4 *jw) {
  const f32 power = 3.2f;
  if (s->bone >= 0) {
    ji[0] = (u16)s->bone;
    *jw = (v4){1.0f, 0.0f, 0.0f, 0.0f};
    return;
  }
  CabPick pk = {{-1, -1, -1, -1}, {-1.0f, -1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};
  for (u32 c = 0; c < s->cand_count; c++) {
    f32 d = cab_dist_to_bone(rig, s->cand[c], p);
    f32 w = 1.0f / (m_powf(d, power) + 1e-6f) * s->bias[c];
    if (w > pk.w[0]) {
      pk.id[3] = pk.id[2]; pk.w[3] = pk.w[2];
      pk.id[2] = pk.id[1]; pk.w[2] = pk.w[1];
      pk.id[1] = pk.id[0]; pk.w[1] = pk.w[0];
      pk.id[0] = (i32)c; pk.w[0] = w;
    } else if (w > pk.w[1]) {
      pk.id[3] = pk.id[2]; pk.w[3] = pk.w[2];
      pk.id[2] = pk.id[1]; pk.w[2] = pk.w[1];
      pk.id[1] = (i32)c; pk.w[1] = w;
    } else if (w > pk.w[2]) {
      pk.id[3] = pk.id[2]; pk.w[3] = pk.w[2];
      pk.id[2] = (i32)c; pk.w[2] = w;
    } else if (w > pk.w[3]) {
      pk.id[3] = (i32)c; pk.w[3] = w;
    }
  }
  f32 tot = 0.0f;
  for (u32 k = 0; k < 4; k++)
    if (pk.id[k] >= 0 && pk.w[k] > 0.0f)
      tot += pk.w[k];
  if (tot <= 0.0f) {
    ji[0] = (u16)s->cand[0];
    *jw = (v4){1.0f, 0.0f, 0.0f, 0.0f};
    return;
  }
  for (u32 k = 0; k < 4; k++) {
    if (pk.id[k] < 0 || pk.w[k] <= 0.0f)
      continue;
    ji[k] = (u16)s->cand[pk.id[k]];
    pk.out[k] = pk.w[k] / tot;
  }
  *jw = (v4){pk.out[0], pk.out[1], pk.out[2], pk.out[3]};
}

hz_internal f32 cab_clamp01(f32 v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

// vertex colour = capsule AO x crevice grime x ground dirt x settled dust x edge wear x per-part tint
hz_internal v4 cab_shade_vertex(const CabNoise *nz, const CabOcc *occ, u32 occ_count, const CabSpec *s, v3 p, v3 n) {
  f32 ao = 1.0f;
  for (u32 o = 0; o < occ_count; o++) {
    const CabOcc *c = &occ[o];
    v3 cp;
    f32 d = cab_seg_dist(p, c->a, c->b, &cp);
    f32 t = d - c->r;
    if (t > 0.09f)
      continue;
    v3 dir = v3_sub(cp, p);
    f32 dl = v3_length(dir);
    if (dl == 0.0f)
      dl = 1.0f;
    dir = v3_scale(dir, 1.0f / dl);
    f32 face = m_maxf(0.0f, v3_dot(n, dir));
    f32 w = (1.0f - m_minf(1.0f, m_maxf(0.0f, t) / 0.09f)) * face * c->k;
    ao *= 1.0f - 0.42f * w;
  }
  f32 grime = (1.0f - ao) * s->grime;
  f32 ground = m_maxf(0.0f, 1.0f - m_maxf(0.0f, p.y - 0.02f) / 0.55f);
  f32 dirt = ground * s->dirt * (0.55f + 0.45f * cab_fbm3(nz, p.x * 26.0f, p.y * 26.0f, p.z * 26.0f, 2));
  f32 dust = m_powf(m_maxf(0.0f, n.y), 2.2f) * (0.35f + 0.65f * cab_fbm3(nz, p.x * 13.0f + 4.0f, p.y * 13.0f, p.z * 13.0f + 9.0f, 2)) * s->dust;
  f32 wear = 0.0f;
  if (s->wear > 0.0f) {
    f32 upness = m_maxf(0.0f, n.y) * 0.55f + m_maxf(0.0f, m_fabsf(n.z)) * 0.45f;
    f32 nzv = cab_fbm3(nz, p.x * 34.0f + 11.0f, p.y * 34.0f, p.z * 34.0f - 7.0f, 3);
    wear = s->wear * upness * m_powf(m_maxf(0.0f, nzv * 0.5f + 0.42f), 1.6f);
  }
  f32 mottle = 1.0f + 0.055f * cab_fbm3(nz, p.x * 9.0f, p.y * 9.0f, p.z * 9.0f, 3);
  f32 r = s->colour.x * ao * mottle, g = s->colour.y * ao * mottle, b = s->colour.z * ao * mottle;
  r = r * (1.0f - grime) + 0.055f * grime;
  g = g * (1.0f - grime) + 0.045f * grime;
  b = b * (1.0f - grime) + 0.036f * grime;
  r = r * (1.0f - dirt * 0.42f) + 0.34f * dirt * 0.42f;
  g = g * (1.0f - dirt * 0.42f) + 0.30f * dirt * 0.42f;
  b = b * (1.0f - dirt * 0.42f) + 0.24f * dirt * 0.42f;
  r = r * (1.0f - dust * 0.30f) + 0.40f * dust * 0.30f;
  g = g * (1.0f - dust * 0.30f) + 0.36f * dust * 0.30f;
  b = b * (1.0f - dust * 0.30f) + 0.30f * dust * 0.30f;
  r = r * (1.0f - wear) + 0.33f * wear;
  g = g * (1.0f - wear) + 0.325f * wear;
  b = b * (1.0f - wear) + 0.31f * wear;
  return (v4){cab_clamp01(r), cab_clamp01(g), cab_clamp01(b), 1.0f};
}

// one part into the merged arrays: positions, normals, tiled uvs, indices, skin weights, vertex colour
hz_internal void cab_merge_part(const CabState *st, CabVariant *v, const CabSpec *s) {
  const PgMesh *m = s->mesh;
  if (!m)
    return;
  const f32 inv = 1.0f / cab_mat_tile[s->mat];
  for (u32 i = 0; i < s->vn; i++) {
    u32 vi = s->vo + i;
    v->pos[vi] = m->pos.items[i];
    v->nrm[vi] = m->nrm.items[i];
    v->uv[vi] = (v2){m->uv.items[i].x * inv, m->uv.items[i].y * inv};
  }
  for (u32 i = 0; i < s->in; i++)
    v->idx[s->io + i] = m->idx.items[i] + s->vo;
  for (u32 i = 0; i < s->vn; i++) {
    u32 vi = s->vo + i;
    cab_bind_vertex(&st->rig, s, v->pos[vi], v->ji + vi * 4, &v->jw[vi]);
    v->col[vi] = cab_shade_vertex(&v->nz, st->occ, st->occ_count, s, v->pos[vi], v->nrm[vi]);
  }
}
