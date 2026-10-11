// Claude of Duty soldier bodies: the ragdoll on engine rigid bodies and swing-twist joints.

#define CAB_RD_CONE (74.0f * (f32)M_PI / 180.0f)
#define CAB_RD_TWIST (38.0f * (f32)M_PI / 180.0f)
#define CAB_RD_RADIUS_RATIO 0.42f
#define CAB_RD_STUB 0.08f
#define CAB_RD_LIFT 0.15f
#define CAB_RD_MASS 82.0f
#define CAB_RD_FRICTION 0.72f
#define CAB_RD_IMPULSE_RADIUS 0.85f
// the ragdoll category plus one bit per bone (the self-collision pair table rides on the masks); bits 0..5 are the game's
#define CAB_RD_BONE_BIT(i) (1u << (6u + (i)))

// a bone's capsule in world space at the moment of death
typedef struct CabRdBone {
  v3 head;
  v3 tail;
  quat rot;
  f32 len;
  f32 radius;
  f32 mass;
} CabRdBone;

// squared distance between two segments (closest points, clamped)
hz_internal f32 cab_seg_seg_d2(v3 p1, v3 q1, v3 p2, v3 q2) {
  v3 d1 = v3_sub(q1, p1), d2 = v3_sub(q2, p2), r = v3_sub(p1, p2);
  f32 a = v3_dot(d1, d1), e = v3_dot(d2, d2), f = v3_dot(d2, r);
  f32 s, t;
  if (a <= 1e-12f && e <= 1e-12f)
    return v3_dot(r, r);
  if (a <= 1e-12f) {
    s = 0.0f;
    t = m_clampf(f / e, 0.0f, 1.0f);
  } else {
    f32 c = v3_dot(d1, r);
    if (e <= 1e-12f) {
      t = 0.0f;
      s = m_clampf(-c / a, 0.0f, 1.0f);
    } else {
      f32 b = v3_dot(d1, d2);
      f32 denom = a * e - b * b;
      s = denom > 1e-12f ? m_clampf((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
      t = (b * s + f) / e;
      if (t < 0.0f) {
        t = 0.0f;
        s = m_clampf(-c / a, 0.0f, 1.0f);
      } else if (t > 1.0f) {
        t = 1.0f;
        s = m_clampf((b - c) / a, 0.0f, 1.0f);
      }
    }
  }
  v3 c1 = v3_add(p1, v3_scale(d1, s)), c2 = v3_add(p2, v3_scale(d2, t));
  v3 dd = v3_sub(c1, c2);
  return v3_dot(dd, dd);
}

// the shared joint points: coordinates equal to the millimetre
hz_internal b32 cab_rd_same_point(v3 a, v3 b) {
  return m_roundf(a.x * 1000.0f) == m_roundf(b.x * 1000.0f) && m_roundf(a.y * 1000.0f) == m_roundf(b.y * 1000.0f) &&
         m_roundf(a.z * 1000.0f) == m_roundf(b.z * 1000.0f);
}

// builds the bodies and joints from the live pose; a Drive death serial change asked for it
hz_internal void cab_ragdoll_build(EcsWorld *w, EcsEntity owner, const CabState *st, CabAnim *a, const CodSoldierDrive *d) {
  const f32 s = d->scale > 0.0f ? d->scale : 1.0f;
  const quat grot = quat_axis_angle((v3){0.0f, 1.0f, 0.0f}, d->yaw);
  const f32 lift = CAB_RD_LIFT * s;
  CabRagdoll rd = {0};
  tctx_temp_allocator_scope(ta, NULL) {
    CabRdBone *b = ALLOC_ARRAY(&ta.allocator, CabRdBone, COD_BONE_COUNT);
    u32 *mask = ALLOC_ARRAY(&ta.allocator, u32, COD_BONE_COUNT);
    f32 vol = 0.0f;
    for (u32 i = 0; i < COD_BONE_COUNT; i++) {
      b[i].head = v3_add(v3_add(d->position, quat_rotate(grot, v3_scale(a->apos[i], s))), (v3){0.0f, lift, 0.0f});
      b[i].rot = quat_mul(grot, a->arot[i]);
    }
    for (u32 i = 0; i < COD_BONE_COUNT; i++) {
      i32 child = st->rig.first_child[i];
      b[i].tail = child >= 0 ? b[child].head : v3_add(b[i].head, v3_scale(quat_rotate(b[i].rot, (v3){0.0f, 1.0f, 0.0f}), CAB_RD_STUB));
      b[i].len = v3_distance(b[i].head, b[i].tail);
      if (b[i].len > 1e-5f)
        b[i].rot = quat_mul(quat_from_to(quat_rotate(b[i].rot, (v3){0.0f, 1.0f, 0.0f}), v3_scale(v3_sub(b[i].tail, b[i].head), 1.0f / b[i].len)), b[i].rot);
      b[i].radius = m_maxf(0.025f, b[i].len * CAB_RD_RADIUS_RATIO);
      b[i].mass = (f32)M_PI * b[i].radius * b[i].radius * b[i].len;
      vol += b[i].mass;
    }
    const f32 total = CAB_RD_MASS * s;
    for (u32 i = 0; i < COD_BONE_COUNT; i++)
      b[i].mass = m_clampf(b[i].mass / (vol / (f32)COD_BONE_COUNT), 0.4f, 2.5f);
    f32 wsum = 0.0f;
    for (u32 i = 0; i < COD_BONE_COUNT; i++)
      wsum += b[i].mass;
    for (u32 i = 0; i < COD_BONE_COUNT; i++)
      b[i].mass = b[i].mass / wsum * total;

    // pairs worth testing: not sharing a joint point, not already overlapping in the bind pose
    for (u32 i = 0; i < COD_BONE_COUNT; i++)
      mask[i] = COD_COLLIDE_WORLD | COD_COLLIDE_GLASS;
    for (u32 i = 0; i < COD_BONE_COUNT; i++)
      for (u32 j = i + 1; j < COD_BONE_COUNT; j++) {
        if (cab_rd_same_point(b[i].head, b[j].head) || cab_rd_same_point(b[i].head, b[j].tail) || cab_rd_same_point(b[i].tail, b[j].head) ||
            cab_rd_same_point(b[i].tail, b[j].tail))
          continue;
        f32 rad = b[i].radius + b[j].radius;
        if (cab_seg_seg_d2(b[i].head, b[i].tail, b[j].head, b[j].tail) < rad * rad * 0.95f)
          continue;
        mask[i] |= CAB_RD_BONE_BIT(j);
        mask[j] |= CAB_RD_BONE_BIT(i);
      }

    // the impulse: 1/(1 + d^2/r^2) falloff on each end point, seen through the mass the end carries
    for (u32 i = 0; i < COD_BONE_COUNT; i++) {
      v3 mid = v3_scale(v3_add(b[i].head, b[i].tail), 0.5f);
      v3 center = mid;
      f32 half = b[i].len * 0.5f;
      v3 axis = quat_rotate(b[i].rot, (v3){0.0f, 1.0f, 0.0f});
      v3 head_pt = v3_sub(center, v3_scale(axis, half));
      v3 tail_pt = v3_add(center, v3_scale(axis, half));
      f32 dv = 0.0f;
      for (u32 e = 0; e < 2; e++) {
        v3 pt = e == 0 ? head_pt : tail_pt;
        v3 dd = v3_sub(pt, v3_add(d->death_point, (v3){0.0f, lift, 0.0f}));
        f32 wgt = 1.0f / (1.0f + v3_lengthsq(dd) / (CAB_RD_IMPULSE_RADIUS * CAB_RD_IMPULSE_RADIUS));
        // an end shared with the neighbouring bone carries both halves
        f32 carried = 0.5f * b[i].mass;
        for (u32 j = 0; j < COD_BONE_COUNT; j++) {
          if (j == i)
            continue;
          v3 pj0 = b[j].head, pj1 = b[j].tail;
          if (cab_rd_same_point(pt, pj0) || cab_rd_same_point(pt, pj1))
            carried += 0.5f * b[j].mass;
        }
        dv += wgt / carried;
      }
      dv *= 0.5f;
      v3 vel = v3_add((v3){d->velocity.x, 0.0f, d->velocity.z}, v3_scale(d->death_impulse, dv));

      EcsEntity e = ecs_entity_create(w);
      PhysicsFilter filter = {.category = COD_COLLIDE_RAGDOLL | CAB_RD_BONE_BIT(i), .mask = mask[i]};
      ecs_component_set(w, e, ColliderBody,
                        {.collider = collider_capsule((v3){0.0f, -half, 0.0f}, (v3){0.0f, half, 0.0f}, b[i].radius),
                         .pos = center,
                         .rot = b[i].rot,
                         .scale = 1.0f,
                         .filter = filter});
      ecs_component_set(w, e, WorldTransform, {.position = center, .rotation = b[i].rot, .scale = V3_ONE});
      ecs_rigidbody_body_add(w, e, (RigidBody){.velocity = {.linear = vel}, .inverse_mass = 1.0f / b[i].mass, .friction = CAB_RD_FRICTION, .restitution = 0.0f});
      ecs_component_set(w, e, NavObstacleIgnore, {0}); // the soldiers step over a body, as the grid did
      ecs_component_set(w, e, CodSurfaceTag, {.surface = COD_SURFACE_FLESH});
      ecs_component_set(w, e, CabRagdollPart, {.owner = owner});
      rd.body[i] = e;
      rd.half_len[i] = half;
    }
    for (u32 i = 0; i < COD_BONE_COUNT; i++) {
      i32 p = cod_rig_parent[i];
      if (p < 0)
        continue;
      v3 axis = v3_normalize(v3_sub(b[i].tail, b[i].head));
      EcsEntity j = ecs_rigidbody_joint_add(w, (RigidBodyJoint){.body_a = rd.body[p],
                                                                 .body_b = rd.body[i],
                                                                 .type = PHYSICS_JOINT_SWING_TWIST,
                                                                 .anchor = b[i].head,
                                                                 .axis = axis,
                                                                 .half_cone = CAB_RD_CONE,
                                                                 .min_angle = -CAB_RD_TWIST,
                                                                 .max_angle = CAB_RD_TWIST});
      ecs_component_set(w, j, CabRagdollPart, {.owner = owner});
    }
  }
  rd.active = true;
  ecs_component_set(w, owner, CabRagdoll, rd);
  a->ragdolled = true;
  a->want_ragdoll = false;
}

// the pose from the bodies: bone = body rotation, head = centre back along the capsule
hz_internal void cab_ragdoll_pose(EcsWorld *w, const CabState *st, const CabVariant *v, const CabRagdoll *rd, CabAnim *a, const CodSoldierDrive *d, CodSoldierPose *pose,
                                  RendererState *rs, m3x4 *palette) {
  const f32 s = d->scale > 0.0f ? d->scale : 1.0f;
  v3 *pos = a->apos;
  quat *rot = a->arot;
  for (u32 i = 0; i < COD_BONE_COUNT; i++) {
    const WorldTransform *wt = ecs_component_get(w, rd->body[i], WorldTransform);
    if (!wt)
      continue;
    rot[i] = wt->rotation;
    pos[i] = v3_sub(wt->position, quat_rotate(wt->rotation, (v3){0.0f, rd->half_len[i], 0.0f}));
  }
  // the transform sits under the hips so the variant's cull box surrounds the body; the palette is relative to it
  v3 origin = v3_sub(pos[COD_BONE_HIPS], (v3){0.0f, cod_rig_bind[COD_BONE_HIPS].y * s, 0.0f});
  for (u32 i = 0; i < COD_BONE_COUNT; i++)
    pos[i] = v3_sub(pos[i], origin);
  cab_palette(&st->rig, pos, rot, s, palette);
  renderer_submesh_instance_set_bones(rs, a->child[0], palette, COD_BONE_COUNT);
  Transform xf = {.position = origin, .rotation = quat_identity(), .scale = V3_ONE};
  cab_set_xform(rs, a, v, &xf);
  for (u32 i = 0; i < COD_BONE_COUNT; i++)
    pose->bones[i] = v3_add(pos[i], origin);
  pose->ready = true;
  pose->ragdoll = true;
  pose->reloading = false;
  pose->vaulting = false;
}

// one caller: creates bodies, joints and entities
ecs_system(cab_ragdoll_system, EcsIn(CodSoldierDrive), EcsInOut(CabAnim), EcsNot(CabRagdoll), EcsResIn(CodBodyState)) {
  const CabState *st = ecs_sys_get_resource(CodBodyState)->s;
  for (i32 i = 0; i < it->count; i++) {
    CabAnim *a = ecs_sys_get_component_mut(CabAnim, i);
    if (!a->want_ragdoll)
      continue;
    cab_ragdoll_build(it->world, it->entities[i], st, a, ecs_sys_get_component(CodSoldierDrive, i));
  }
}

// one caller: destroys the bodies and joints whose soldier is gone
ecs_system(cab_ragdoll_gc_system, EcsIn(CabRagdollPart)) {
  for (i32 i = 0; i < it->count; i++) {
    const CabRagdollPart *p = ecs_sys_get_component(CabRagdollPart, i);
    if (!ecs_entity_exists(it->world, p->owner))
      ecs_entity_destroy(it->world, it->entities[i]);
  }
}
