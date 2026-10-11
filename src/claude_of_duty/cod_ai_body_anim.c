// Claude of Duty soldier bodies: the animation runtime. A layered blend over clips, then aim, look-at, support-hand and foot IK. Everything runs in the actor's frame (unit scale, yaw removed); only the ground probe and the outputs cross to world space.

#define CAB_DEG ((f32)M_PI / 180.0f)

// Euler 'XYZ': qx * qy * qz
hz_internal quat cab_euler_xyz_deg(v3 e) {
  quat qx = quat_axis_angle((v3){1.0f, 0.0f, 0.0f}, e.x * CAB_DEG);
  quat qy = quat_axis_angle((v3){0.0f, 1.0f, 0.0f}, e.y * CAB_DEG);
  quat qz = quat_axis_angle((v3){0.0f, 0.0f, 1.0f}, e.z * CAB_DEG);
  return quat_mul(qx, quat_mul(qy, qz));
}

// the bones from `from` down: world = parent world * local (actor frame)
hz_internal void cab_world_from(CabAnim *a, u32 from) {
  for (u32 j = from; j < COD_BONE_COUNT; j++) {
    i32 p = cod_rig_parent[j];
    if (p < 0) {
      a->apos[j] = a->lpos[j];
      a->arot[j] = a->lrot[j];
    } else {
      a->apos[j] = v3_add(a->apos[p], quat_rotate(a->arot[p], a->lpos[j]));
      a->arot[j] = quat_mul(a->arot[p], a->lrot[j]);
    }
  }
}

// rotate bone i in world space by dq, keeping its position
hz_internal void cab_apply_world(CabAnim *a, u32 i, quat dq) {
  i32 p = cod_rig_parent[i];
  quat pq = p >= 0 ? a->arot[p] : quat_identity();
  quat cur = quat_mul(pq, a->lrot[i]);
  cur = quat_mul(dq, cur);
  a->lrot[i] = quat_mul(quat_conjugate(pq), cur);
  cab_world_from(a, i);
}

// point bone i's +Y along dir (unit), preserving twist
hz_internal void cab_aim_bone(CabAnim *a, u32 i, v3 dir) {
  v3 cur = quat_rotate(a->arot[i], (v3){0.0f, 1.0f, 0.0f});
  cab_apply_world(a, i, quat_from_to(cur, dir));
}

// analytic two-bone IK on [upper, lower, end]; keeps the bones' twist and never over-extends
hz_internal void cab_two_bone(CabAnim *a, u32 iu, u32 il, u32 ie, v3 target, v3 pole) {
  v3 A = a->apos[iu], B = a->apos[il], C = a->apos[ie];
  f32 l1 = v3_distance(A, B), l2 = v3_distance(B, C);
  if (l1 < 1e-5f || l2 < 1e-5f)
    return;
  v3 dir = v3_sub(target, A);
  f32 d = v3_length(dir);
  if (d < 1e-5f)
    return;
  dir = v3_scale(dir, 1.0f / d);
  f32 mn = m_fabsf(l1 - l2) + 1e-4f, mx = l1 + l2 - 1e-4f;
  d = m_minf(mx, m_maxf(mn, d));
  f32 aa = (l1 * l1 - l2 * l2 + d * d) / (2.0f * d);
  f32 h = m_sqrtf(m_maxf(0.0f, l1 * l1 - aa * aa));
  v3 perp = v3_sub(pole, v3_scale(dir, v3_dot(pole, dir)));
  if (v3_lengthsq(perp) < 1e-8f)
    perp = v3_add((v3){0.0f, 1.0f, 0.0f}, v3_scale(dir, -dir.y));
  perp = v3_normalize(perp);
  v3 e = v3_add(v3_add(A, v3_scale(dir, aa)), v3_scale(perp, h));
  v3 to_e = v3_sub(e, A);
  if (v3_lengthsq(to_e) < 1e-10f)
    return;
  cab_aim_bone(a, iu, v3_normalize(to_e));
  v3 B2 = a->apos[il];
  v3 to_t = v3_sub(target, B2);
  if (v3_lengthsq(to_t) < 1e-10f)
    return;
  cab_aim_bone(a, il, v3_normalize(to_t));
}

// ---- IK passes ----

hz_internal const u32 cab_aim_chain[3] = {COD_BONE_SPINE, COD_BONE_SPINE1, COD_BONE_SPINE2};
hz_internal const f32 cab_aim_spread[3] = {0.12f, 0.34f, 0.54f};
hz_internal const u32 cab_look_chain[2] = {COD_BONE_NECK, COD_BONE_HEAD};
hz_internal const f32 cab_look_frac[2] = {0.4f, 0.6f};
hz_internal const u32 cab_legs[2][3] = {{COD_BONE_UP_LEG_R, COD_BONE_LEG_R, COD_BONE_FOOT_R}, {COD_BONE_UP_LEG_L, COD_BONE_LEG_L, COD_BONE_FOOT_L}};

hz_internal void cab_aim_ik(CabAnim *a, const CabAnchors *anc, v3 target, f32 weight) {
  for (u32 iter = 0; iter < 2; iter++) {
    v3 bore = v3_normalize(quat_rotate(a->arot[COD_BONE_HAND_R], anc->bore));
    v3 muzzle = v3_add(a->apos[COD_BONE_HAND_R], quat_rotate(a->arot[COD_BONE_HAND_R], anc->muzzle));
    v3 want = v3_sub(target, muzzle);
    if (v3_lengthsq(want) < 1e-6f)
      return;
    want = v3_normalize(want);
    f32 dot = m_minf(1.0f, m_maxf(-1.0f, v3_dot(bore, want)));
    f32 ang = m_acosf(dot) * weight;
    if (ang < 0.0015f)
      return;
    f32 max_iter = iter == 0 ? 0.9f : 0.35f;
    if (ang > max_iter)
      ang = max_iter;
    v3 axis = v3_cross(bore, want);
    if (v3_lengthsq(axis) < 1e-10f)
      return;
    axis = v3_normalize(axis);
    for (u32 k = 0; k < 3; k++)
      cab_apply_world(a, cab_aim_chain[k], quat_axis_angle(axis, ang * cab_aim_spread[k]));
  }
}

hz_internal void cab_look_at(CabAnim *a, v3 target, f32 weight) {
  for (u32 k = 0; k < 2; k++) {
    u32 bi = cab_look_chain[k];
    v3 fwd = quat_rotate(a->arot[bi], (v3){0.0f, 0.0f, 1.0f});
    v3 want = v3_sub(target, a->apos[bi]);
    if (v3_lengthsq(want) < 1e-6f)
      return;
    want = v3_normalize(want);
    f32 dot = m_minf(1.0f, m_maxf(-1.0f, v3_dot(fwd, want)));
    f32 ang = m_acosf(dot) * weight * cab_look_frac[k];
    if (ang < 0.002f)
      continue;
    if (ang > 0.5f)
      ang = 0.5f;
    v3 axis = v3_cross(fwd, want);
    if (v3_lengthsq(axis) < 1e-10f)
      continue;
    cab_apply_world(a, bi, quat_axis_angle(v3_normalize(axis), ang));
  }
}

hz_internal v3 cab_hand_point(const CabAnim *a, v3 local) { return v3_add(a->apos[COD_BONE_HAND_R], quat_rotate(a->arot[COD_BONE_HAND_R], local)); }

// the support hand back on the handguard, or following the magazine during a reload
hz_internal void cab_support_hand_ik(CabAnim *a, const CabAnchors *anc) {
  if (a->vault_t >= 0.0f)
    return;
  v3 t;
  if (a->reload_t >= 0.0f) {
    f32 p = a->reload_t / a->reload_dur;
    v3 mag = cab_hand_point(a, anc->mag);
    v3 grip = cab_hand_point(a, anc->foregrip);
    v3 chest = {0.02f, 1.19f, 0.17f};
    if (p < 0.18f)
      t = v3_lerp(grip, mag, p / 0.18f);
    else if (p < 0.42f)
      t = v3_lerp(mag, chest, (p - 0.18f) / 0.24f);
    else if (p < 0.62f)
      t = v3_lerp(chest, mag, (p - 0.42f) / 0.20f);
    else if (p < 0.78f)
      t = mag;
    else
      t = v3_lerp(mag, grip, (p - 0.78f) / 0.22f);
  } else {
    t = cab_hand_point(a, anc->foregrip);
  }
  cab_two_bone(a, COD_BONE_UPPER_ARM_L, COD_BONE_FOREARM_L, COD_BONE_HAND_L, t, (v3){0.6f, -1.0f, -0.25f});
}

// world <-> actor frame (yaw about +Y, uniform scale)
hz_internal v3 cab_to_world(const CabProbe *pc, v3 p) { return v3_add(pc->gpos, quat_rotate(pc->grot, v3_scale(p, pc->scale))); }

hz_internal v3 cab_to_actor(const CabProbe *pc, v3 p) {
  return v3_scale(quat_rotate(quat_conjugate(pc->grot), v3_sub(p, pc->gpos)), 1.0f / pc->scale);
}

// a ray down from (x, from_y, z) into the static layer, 3.2 m; world space in and out
hz_internal b32 cab_probe_ground(const CabProbe *pc, v3 from, f32 *out_y, v3 *out_n) {
  if (!pc->layer)
    return false;
  PhysicsRaycastResult r = {0};
  i32 idx = 0;
  v3 to = {from.x, from.y - 3.2f, from.z};
  if (!physics_raycast_layer(from, to, (CollisionLayer *)&pc->layer->layer, PHYSICS_QUERY_FILTER_ALL, &r, &idx))
    return false;
  *out_y = r.position.y;
  *out_n = r.normal;
  return true;
}

hz_internal void cab_foot_ik(CabAnim *a, const CabProbe *pc) {
  if (!pc->layer)
    return;
  const f32 ankle_h = 0.088f;
  f32 drop = 0.0f;
  for (u32 k = 0; k < 2; k++) {
    v3 ankle = a->apos[cab_legs[k][2]];
    v3 aw = cab_to_world(pc, ankle);
    f32 gy;
    v3 gn;
    if (!cab_probe_ground(pc, (v3){aw.x, aw.y + 0.55f * pc->scale, aw.z}, &gy, &gn)) {
      a->foot_y[k] = ankle.y;
      a->foot_n[k] = (v3){0.0f, 1.0f, 0.0f};
      continue;
    }
    f32 want = (gy - pc->gpos.y) / pc->scale + ankle_h;
    a->foot_y[k] = want;
    a->foot_n[k] = quat_rotate(quat_conjugate(pc->grot), gn);
    f32 d = want - ankle.y;
    if (d < drop)
      drop = d;
  }
  drop = m_maxf(-0.32f, drop);
  if (drop < -0.002f) {
    a->lpos[0].y += drop;
    cab_world_from(a, 0);
  }
  for (u32 k = 0; k < 2; k++) {
    v3 ankle = a->apos[cab_legs[k][2]];
    v3 target = {ankle.x, m_maxf(a->foot_y[k], ankle.y - 0.001f), ankle.z};
    v3 pole = {k == 0 ? -0.12f : 0.12f, 0.05f, 1.0f};
    cab_two_bone(a, cab_legs[k][0], cab_legs[k][1], cab_legs[k][2], target, pole);
    v3 n = a->foot_n[k];
    if (n.y < 0.999f) {
      u32 foot = cab_legs[k][2];
      v3 up = quat_rotate(a->arot[foot], (v3){0.0f, 0.0f, 1.0f});
      f32 dot = m_minf(1.0f, m_maxf(-1.0f, v3_dot(up, n)));
      f32 ang = m_acosf(dot);
      if (ang > 0.35f)
        ang = 0.35f;
      v3 axis = v3_cross(up, n);
      if (v3_lengthsq(axis) > 1e-10f)
        cab_apply_world(a, foot, quat_axis_angle(v3_normalize(axis), ang));
    }
  }
}

hz_internal void cab_update_muzzle(CabAnim *a, const CabAnchors *anc) {
  a->muzzle = cab_hand_point(a, anc->muzzle);
  a->eject = cab_hand_point(a, anc->eject);
  a->muzzle_dir = v3_normalize(quat_rotate(a->arot[COD_BONE_HAND_R], anc->bore));
}

// ---- state and update ----

void cab_anim_reset(CabAnim *a, const CodSoldierDrive *d, u32 variant) {
  mem_zero(a, sizeof(*a));
  a->enabled = true;
  a->ground_slot = -1;
  a->variant = variant;
  a->clip = a->prev_clip = d->clip;
  a->blend = 1.0f;
  a->recoil_t = a->hit_t = a->reload_t = a->vault_t = a->turn_t = -1.0f;
  a->reload_dur = 2.4f;
  a->vault_dur = 0.85f;
  a->turn_dir = 1.0f;
  a->seen_fire = d->fire_serial;
  a->seen_hit = d->hit_serial;
  a->seen_reload = d->reload_serial;
  a->seen_vault = d->vault_serial;
  a->seen_turn = d->turn_serial;
  a->seen_death = d->death_serial;
}

// the animator state and the one-shot triggers, taken every frame so a skipped evaluation still sees them
void cab_anim_state(CabAnim *a, const CodSoldierDrive *d) {
  if (d->clip != a->clip) {
    a->prev_clip = a->clip;
    a->blend = 0.0f;
    a->clip = d->clip;
  }
  if (d->fire_serial != a->seen_fire) {
    a->seen_fire = d->fire_serial;
    a->recoil_t = 0.0f;
    a->recoil_k = d->fire_strength;
  }
  if (d->hit_serial != a->seen_hit) {
    a->seen_hit = d->hit_serial;
    a->hit_t = 0.0f;
    u32 region = CAB_HIT_TORSO;
    switch (d->hit_zone) {
    case COD_ZONE_HEAD: region = CAB_HIT_HEAD; break;
    case COD_ZONE_ARM: region = d->hit_limb < 0.0f ? CAB_HIT_ARM_R : CAB_HIT_ARM_L; break;
    case COD_ZONE_LEG: region = d->hit_limb < 0.0f ? CAB_HIT_LEG_R : CAB_HIT_LEG_L; break;
    default: break;
    }
    a->hit_region = region;
    a->hit_side = d->hit_side;
    a->hit_k = d->hit_strength;
  }
  if (d->reload_serial != a->seen_reload) {
    a->seen_reload = d->reload_serial;
    a->reload_t = 0.0f;
    a->reload_dur = d->reload_duration;
  }
  if (d->vault_serial != a->seen_vault) {
    a->seen_vault = d->vault_serial;
    a->vault_t = 0.0f;
    a->vault_dur = d->vault_duration;
  }
  if (d->turn_serial != a->seen_turn) {
    a->seen_turn = d->turn_serial;
    if (a->turn_t < 0.0f) {
      a->turn_t = 0.0f;
      a->turn_dir = d->turn_dir >= 0.0f ? 1.0f : -1.0f;
    }
  }
}

// the animator update: the pose write and the IK passes
void cab_anim_update(CabAnim *a, const CabRig *rig, const CabAnchors *anc, const CodSoldierDrive *d, f32 dt, const CabProbe *pc) {
  CabPoser *P = &a->poser;
  const b32 vaulting = a->vault_t >= 0.0f;
  f32 stride_hz;
  switch (a->clip) {
  case COD_CLIP_RUN: stride_hz = m_maxf(1.1f, d->speed / 2.05f); break;
  case COD_CLIP_WALK: stride_hz = m_maxf(0.55f, d->speed / 1.42f); break;
  case COD_CLIP_CROUCH_WALK: stride_hz = m_maxf(0.4f, d->speed / 0.95f); break;
  default: stride_hz = 0.19f; break;
  }
  a->phase = m_fmodf(a->phase + dt * stride_hz, 1.0f);
  if (a->blend < 1.0f)
    a->blend = m_minf(1.0f, a->blend + dt / 0.18f);

  mem_zero(P, sizeof(*P));
  P->w = 1.0f;
  if (a->blend < 1.0f) {
    P->w = 1.0f - a->blend;
    cab_clip_play(a->prev_clip, P, a->phase);
    P->w = a->blend;
    cab_clip_play(a->clip, P, a->phase);
  } else {
    cab_clip_play(a->clip, P, a->phase);
  }

  P->w = 1.0f;
  if (d->aim_weight > 0.0f && !vaulting)
    cab_clip_aim_add(P, d->aim_weight * (1.0f - (a->reload_t >= 0.0f ? 0.6f : 0.0f)));
  if (d->suppress > 0.0f)
    cab_clip_suppress_add(P, m_minf(1.0f, d->suppress));
  if (a->recoil_t >= 0.0f) {
    cab_clip_recoil_add(P, a->recoil_t, a->recoil_k);
    a->recoil_t += dt;
    if (a->recoil_t > 0.3f)
      a->recoil_t = -1.0f;
  }
  if (a->hit_t >= 0.0f) {
    cab_clip_hit_add(P, a->hit_region, a->hit_t, a->hit_side, a->hit_k);
    a->hit_t += dt;
    if (a->hit_t > 0.55f)
      a->hit_t = -1.0f;
  }
  if (a->reload_t >= 0.0f) {
    cab_clip_reload_add(P, a->reload_t / a->reload_dur);
    a->reload_t += dt;
    if (a->reload_t > a->reload_dur)
      a->reload_t = -1.0f;
  }
  if (a->turn_t >= 0.0f) {
    cab_clip_turn_step(P, a->turn_t / 0.42f, a->turn_dir);
    a->turn_t += dt;
    if (a->turn_t > 0.42f)
      a->turn_t = -1.0f;
  }
  if (a->vault_t >= 0.0f) {
    P->w = 1.0f;
    cab_clip_vault(P, a->vault_t / a->vault_dur);
    a->vault_t += dt;
    if (a->vault_t > a->vault_dur)
      a->vault_t = -1.0f;
  }

  // the pose write: bind locals with the euler deltas, the hip offset on the root
  for (u32 i = 0; i < COD_BONE_COUNT; i++) {
    v3 e = P->d[i];
    a->lrot[i] = (e.x != 0.0f || e.y != 0.0f || e.z != 0.0f) ? quat_mul(rig->local_quat[i], cab_euler_xyz_deg(e)) : rig->local_quat[i];
    a->lpos[i] = i == 0 ? v3_add(rig->local_pos[i], P->hip) : rig->local_pos[i];
  }
  cab_world_from(a, 0);

  const b32 vaulting_now = a->vault_t >= 0.0f;
  if (!vaulting_now)
    cab_foot_ik(a, pc);
  if (d->has_aim && d->aim_weight > 0.01f && !vaulting_now)
    cab_aim_ik(a, anc, cab_to_actor(pc, d->aim_target), d->aim_weight);
  if (d->has_look)
    cab_look_at(a, cab_to_actor(pc, d->look_target), m_maxf(0.35f, d->aim_weight));
  cab_support_hand_ik(a, anc);
  cab_update_muzzle(a, anc);
  a->posed = true;
}

// the palette of the actor-frame pose: bone matrix times the inverse bind
hz_internal void cab_palette(const CabRig *rig, const v3 *pos, const quat *rot, f32 scale, m3x4 *out) {
  for (u32 i = 0; i < COD_BONE_COUNT; i++) {
    quat r = quat_mul(rot[i], quat_conjugate(rig->bind_quat[i]));
    v3 t = v3_sub(pos[i], quat_rotate(r, v3_scale(rig->bind_pos[i], scale)));
    out[i] = m3x4_from_m4(m4_trs(t, r, (v3){scale, scale, scale}));
  }
}


// the skinned submeshes take the soldier's transform together (their cull box is the variant's envelope, fixed at instantiate)
hz_internal void cab_set_xform(RendererState *rs, const CabAnim *a, const CabVariant *v, const Transform *xf) {
  UNUSED(v);
  for (u32 c = 0; c < a->child_count; c++)
    renderer_submesh_instance_set_transform(rs, a->child[c], xf);
}

