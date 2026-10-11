// Claude of Duty soldier bodies: the rig, the variants and their part lists, skin weights and the baked vertex shading.

// ----------------------------------------------------------------- rig ---

#define CAB_LEAF_STUB 0.075f

// the fixed leaf directions and up hints
hz_internal v3 cab_leaf_dir(u32 bone) {
  switch (bone) {
  case COD_BONE_FINGERS_R: return v3_normalize((v3){0.30f, -0.35f, 0.89f});
  case COD_BONE_FINGERS_L: return v3_normalize((v3){-0.32f, -0.30f, 0.90f});
  case COD_BONE_TOE_R:
  case COD_BONE_TOE_L: return (v3){0.0f, 1.0f, 0.0f};
  default: return V3_ZERO;
  }
}

hz_internal b32 cab_has_up_hint(u32 bone) {
  return bone == COD_BONE_FOOT_R || bone == COD_BONE_TOE_R || bone == COD_BONE_FOOT_L || bone == COD_BONE_TOE_L;
}

// Rig constructor: tails, +Y-down-the-bone world rotations and the local transforms
void cab_rig_init(CabRig *rig) {
  for (u32 i = 0; i < COD_BONE_COUNT; i++) {
    rig->first_child[i] = -1;
    rig->bind_pos[i] = cod_rig_bind[i];
  }
  for (u32 i = 0; i < COD_BONE_COUNT; i++) {
    i32 p = cod_rig_parent[i];
    if (p >= 0 && rig->first_child[p] < 0)
      rig->first_child[p] = (i32)i;
  }
  for (u32 i = 0; i < COD_BONE_COUNT; i++) {
    v3 tail;
    i32 kid = rig->first_child[i];
    if (kid >= 0) {
      tail = cod_rig_bind[kid];
      u32 kids = 0;
      for (u32 k = 0; k < COD_BONE_COUNT; k++)
        if (cod_rig_parent[k] == (i8)i)
          kids++;
      if (kids > 1) {
        // a branch point aims at the first chain child that is not a clavicle or a leg
        for (u32 k = 0; k < COD_BONE_COUNT; k++)
          if (cod_rig_parent[k] == (i8)i && k != COD_BONE_CLAVICLE_R && k != COD_BONE_CLAVICLE_L && k != COD_BONE_UP_LEG_R && k != COD_BONE_UP_LEG_L) {
            tail = cod_rig_bind[k];
            break;
          }
      }
    } else {
      v3 d = cab_leaf_dir(i);
      if (v3_lengthsq(d) < 1e-9f)
        d = v3_normalize(v3_sub(cod_rig_bind[i], cod_rig_bind[cod_rig_parent[i]]));
      tail = v3_add(cod_rig_bind[i], v3_scale(d, CAB_LEAF_STUB));
    }
    rig->tail[i] = tail;
    v3 y = v3_sub(tail, cod_rig_bind[i]);
    if (v3_lengthsq(y) < 1e-10f)
      y = (v3){0.0f, 1.0f, 0.0f};
    y = v3_normalize(y);
    v3 up = cab_has_up_hint(i) ? (v3){0.0f, 1.0f, 0.0f} : (v3){0.0f, 0.0f, 1.0f};
    if (m_fabsf(v3_dot(up, y)) > 0.985f)
      up = (v3){1.0f, 0.0f, 0.0f};
    v3 x = v3_normalize(v3_cross(y, up));
    v3 z = v3_normalize(v3_cross(x, y));
    rig->bind_quat[i] = quat_from_m3((m3){{{x.x, x.y, x.z}, {y.x, y.y, y.z}, {z.x, z.y, z.z}}});
  }
  for (u32 i = 0; i < COD_BONE_COUNT; i++) {
    i32 p = cod_rig_parent[i];
    if (p < 0) {
      rig->local_pos[i] = cod_rig_bind[i];
      rig->local_quat[i] = rig->bind_quat[i];
    } else {
      quat inv = quat_conjugate(rig->bind_quat[p]);
      rig->local_pos[i] = quat_rotate(inv, v3_sub(cod_rig_bind[i], cod_rig_bind[p]));
      rig->local_quat[i] = quat_mul(inv, rig->bind_quat[i]);
    }
  }
}

hz_internal f32 cab_dist_to_bone(const CabRig *rig, u32 bone, v3 p) {
  v3 a = rig->bind_pos[bone], b = rig->tail[bone];
  v3 d = v3_sub(b, a);
  f32 l2 = v3_lengthsq(d);
  f32 t = l2 > 1e-12f ? v3_dot(v3_sub(p, a), d) / l2 : 0.0f;
  t = m_clampf(t, 0.0f, 1.0f);
  return v3_length(v3_sub(p, v3_add(a, v3_scale(d, t))));
}

// ------------------------------------------------------------ variants ---

hz_internal const CabVariantDef cab_variants[CAB_VARIANTS] = {
    {.name = "vanguard",
     .camo = CAB_SET_CAMO_ARID,
     .cloth_tint = {1.03f, 1.0f, 0.94f},
     .gear_tint = {1.08f, 0.98f, 0.80f},
     .plate_tint = {1.02f, 0.96f, 0.84f},
     .skin_tint = {1.0f, 0.94f, 0.88f},
     .helmet = true,
     .helmet_cover = true,
     .helmet_tint = {0.72f, 0.72f, 0.68f},
     .goggles = true,
     .goggles_down = true,
     .face_wrap = true,
     .knee_pads = true,
     .full_carrier = true,
     .bulk = 1.0f,
     .scale = 1.0f},
    {.name = "irregular",
     .camo = CAB_SET_CAMO_WOODLAND,
     .cloth_tint = {0.98f, 1.02f, 0.94f},
     .gear_tint = {0.92f, 0.96f, 0.74f},
     .plate_tint = {0.90f, 0.94f, 0.80f},
     .skin_tint = {0.86f, 0.80f, 0.74f},
     .head_wrap = true,
     .shades = true,
     .face_wrap = true,
     .long_weapon = true,
     .bulk = 0.94f,
     .scale = 0.985f},
    {.name = "breacher",
     .camo = CAB_SET_CAMO_URBAN,
     .cloth_tint = {0.98f, 0.99f, 1.02f},
     .gear_tint = {0.84f, 0.86f, 0.90f},
     .plate_tint = {0.86f, 0.88f, 0.92f},
     .skin_tint = {1.06f, 0.98f, 0.92f},
     .helmet = true,
     .helmet_tint = {0.82f, 0.83f, 0.86f},
     .goggles = true,
     .face_wrap = true,
     .mask_hard = true,
     .knee_pads = true,
     .full_carrier = true,
     .bulk = 1.06f,
     .scale = 1.025f},
};

const CabVariantDef *cab_variant_def(u32 variant) { return &cab_variants[variant < CAB_VARIANTS ? variant : 0]; }

// tile sizes in metres per texture tile
hz_internal const f32 cab_mat_tile[CAB_MAT_COUNT] = {
    [CAB_MAT_CLOTH] = 0.78f, // the cloth tile
    [CAB_MAT_PLATE] = 0.42f, [CAB_MAT_GEAR] = 0.26f, [CAB_MAT_BOOT] = 0.26f, [CAB_MAT_SKIN] = 0.20f,
    [CAB_MAT_POLYMER] = 0.15f, [CAB_MAT_STEEL] = 0.18f, [CAB_MAT_RUBBER] = 0.11f, [CAB_MAT_GLASS] = 1.0f,
};

// the colour tables
hz_internal const v3 CAB_G_WEBBING = {0.70f, 0.70f, 0.70f};
hz_internal const v3 CAB_G_POUCH = {0.84f, 0.84f, 0.84f};
hz_internal const v3 CAB_G_POUCH_ALT = {0.76f, 0.76f, 0.76f};
hz_internal const v3 CAB_G_DUMP = {0.72f, 0.72f, 0.72f};
hz_internal const v3 CAB_G_BELT = {0.62f, 0.61f, 0.57f};
hz_internal const v3 CAB_G_PAD = {0.55f, 0.55f, 0.55f};
hz_internal const v3 CAB_G_STRAP = {0.56f, 0.56f, 0.56f};
hz_internal const v3 CAB_G_WRAP = {0.56f, 0.54f, 0.50f};
hz_internal const v3 CAB_G_GLOVE = {0.38f, 0.372f, 0.363f};
hz_internal const v3 CAB_G_BOOT = {0.22f, 0.209f, 0.198f};
hz_internal const v3 CAB_G_LACE = {0.21f, 0.204f, 0.198f};
hz_internal const v3 CAB_G_MASK = {0.62f, 0.63f, 0.66f};

// B.add defaults: colour 1, grime 0.5, dirt 0.5, dust 0.22, wear 0
hz_internal CabSpec *cab_new(CabVariant *v, u32 kind, u32 mat) {
  assert(v->spec_count < CAB_SPECS_MAX);
  CabSpec *s = &v->spec[v->spec_count++];
  mem_zero(s, sizeof(*s));
  s->kind = kind;
  s->mat = mat;
  s->bone = -1;
  s->colour = (v3){1.0f, 1.0f, 1.0f};
  s->grime = 0.5f;
  s->dirt = 0.5f;
  s->dust = 0.22f;
  s->side = 1.0f;
  s->job = kind < CAB_PART_WPN_STEEL || kind == CAB_PART_SLING;
  s->bend = (v3){0.0f, 0.0f, -1.0f};
  return s;
}

hz_internal void cab_look(CabSpec *s, v3 colour, f32 grime, f32 dirt, f32 dust, f32 wear) {
  s->colour = colour;
  s->grime = grime;
  if (dirt >= 0.0f)
    s->dirt = dirt;
  if (dust >= 0.0f)
    s->dust = dust;
  s->wear = wear;
}

hz_internal void cab_cand(CabSpec *s, u32 bone, f32 bias) {
  assert(s->cand_count < CAB_CAND_MAX);
  s->cand[s->cand_count] = bone;
  s->bias[s->cand_count++] = bias;
}

hz_internal void cab_radii(CabSpec *s, u32 count, const f32 *radii) {
  s->radii_count = count;
  for (u32 i = 0; i < count; i++)
    s->radii[i] = radii[i];
}

hz_internal v3 cab_bind(u32 bone) { return cod_rig_bind[bone]; }

hz_internal CabPouch cab_pouch_of(f32 hx, f32 hy, f32 hz, f32 x, f32 y, f32 z) {
  return (CabPouch){.hx = hx, .hy = hy, .hz = hz, .x = x, .y = y, .z = z};
}

hz_internal const f32 cab_variant_specs_sleeve_r[7] = {0.050f, 0.062f, 0.056f, 0.050f, 0.046f, 0.042f, 0.038f};
hz_internal const f32 cab_variant_specs_pad_r[3] = {0.050f, 0.054f, 0.050f};
hz_internal const f32 cab_variant_specs_leg_r[7] = {0.090f, 0.085f, 0.076f, 0.068f, 0.062f, 0.060f, 0.064f};
// the soldier's part list order: one add per part, with the rng draws each build makes
void cab_variant_specs(CabVariant *v) {
  const CabVariantDef *V = v->def;
  v->spec_count = 0;
  const v3 shR = cab_bind(COD_BONE_UPPER_ARM_R), elR = cab_bind(COD_BONE_FOREARM_R), wrR = cab_bind(COD_BONE_HAND_R);
  const v3 shL = cab_bind(COD_BONE_UPPER_ARM_L), elL = cab_bind(COD_BONE_FOREARM_L), wrL = cab_bind(COD_BONE_HAND_L);
  const v3 hipR = cab_bind(COD_BONE_UP_LEG_R), knR = cab_bind(COD_BONE_LEG_R), anR = cab_bind(COD_BONE_FOOT_R);
  const v3 hipL = cab_bind(COD_BONE_UP_LEG_L), knL = cab_bind(COD_BONE_LEG_L), anL = cab_bind(COD_BONE_FOOT_L);
  const v3 head = cab_bind(COD_BONE_HEAD);
  CabSpec *s;

  // uniform
  s = cab_new(v, CAB_PART_JACKET, CAB_MAT_CLOTH);
  s->scalar = V->bulk;
  cab_cand(s, COD_BONE_HIPS, 1.0f); cab_cand(s, COD_BONE_SPINE, 1.0f); cab_cand(s, COD_BONE_SPINE1, 1.0f); cab_cand(s, COD_BONE_SPINE2, 1.0f); cab_cand(s, COD_BONE_NECK, 0.8f); cab_cand(s, COD_BONE_CLAVICLE_R, 0.55f); cab_cand(s, COD_BONE_CLAVICLE_L, 0.55f); cab_cand(s, COD_BONE_UPPER_ARM_R, 0.30f); cab_cand(s, COD_BONE_UPPER_ARM_L, 0.30f);
  cab_look(s, (v3){1, 1, 1}, 0.85f, 0.20f, 0.34f, 0.08f);

  s = cab_new(v, CAB_PART_PELVIS, CAB_MAT_CLOTH);
  cab_cand(s, COD_BONE_HIPS, 1.0f); cab_cand(s, COD_BONE_SPINE, 0.7f); cab_cand(s, COD_BONE_UP_LEG_R, 0.5f); cab_cand(s, COD_BONE_UP_LEG_L, 0.5f);
  cab_look(s, (v3){0.97f, 0.97f, 0.97f}, 0.9f, 0.35f, 0.2f, 0.0f);

  s = cab_new(v, CAB_PART_COLLAR, CAB_MAT_CLOTH);
  cab_cand(s, COD_BONE_NECK, 1.0f); cab_cand(s, COD_BONE_SPINE2, 0.8f); cab_cand(s, COD_BONE_HEAD, 0.3f);
  cab_look(s, (v3){0.92f, 0.92f, 0.91f}, 1.0f, -1.0f, 0.3f, 0.0f);

  // sleeves: right then left
  for (u32 side_i = 0; side_i < 2; side_i++) {
    const b32 right = side_i == 0;
    const v3 sh = right ? shR : shL, el = right ? elR : elL, wr = right ? wrR : wrL;
    const f32 side = right ? -1.0f : 1.0f;
    const u32 clav = right ? COD_BONE_CLAVICLE_R : COD_BONE_CLAVICLE_L, upper = right ? COD_BONE_UPPER_ARM_R : COD_BONE_UPPER_ARM_L,
              fore = right ? COD_BONE_FOREARM_R : COD_BONE_FOREARM_L, hand = right ? COD_BONE_HAND_R : COD_BONE_HAND_L;
    s = cab_new(v, CAB_PART_SHOULDER_CAP, CAB_MAT_CLOTH);
    s->p0 = sh;
    s->side = side;
    cab_cand(s, clav, 0.8f); cab_cand(s, upper, 1.0f); cab_cand(s, COD_BONE_SPINE2, 0.4f);
    cab_look(s, (v3){1, 1, 1}, 0.7f, -1.0f, 0.5f, 0.06f);

    s = cab_new(v, CAB_PART_SLEEVE, CAB_MAT_CLOTH);
    s->p0 = (v3){sh.x + side * 0.012f, sh.y + 0.055f, sh.z};
    s->p1 = el;
    s->p2 = wr;
    cab_radii(s, 7, cab_variant_specs_sleeve_r);
    s->rings = 22;
    s->seg = 16;
    s->fold = 0.0016f;
    s->crease = 0.0030f;
    cab_cand(s, clav, 0.5f); cab_cand(s, upper, 1.0f); cab_cand(s, fore, 1.0f); cab_cand(s, hand, 0.7f); cab_cand(s, COD_BONE_SPINE2, 0.25f);
    cab_look(s, (v3){1, 1, 1}, 0.8f, 0.15f, 0.3f, 0.12f);

    s = cab_new(v, CAB_PART_ELBOW_PAD, CAB_MAT_GEAR);
    s->p0 = (v3){el.x * 0.98f + sh.x * 0.02f, el.y + 0.05f, el.z - 0.005f};
    s->p1 = el;
    s->p2 = (v3){el.x * 0.98f + wr.x * 0.02f, el.y - 0.05f, el.z + 0.004f};
    cab_radii(s, 3, cab_variant_specs_pad_r);
    s->rings = 5;
    s->seg = 14;
    s->fold = 0.001f;
    cab_cand(s, upper, 1.0f); cab_cand(s, fore, 1.0f);
    cab_look(s, CAB_G_PAD, 1.0f, 0.25f, 0.22f, 0.16f);
  }

  // trousers, pockets, pads, boots: right then left
  for (u32 side_i = 0; side_i < 2; side_i++) {
    const b32 right = side_i == 0;
    const v3 hip = right ? hipR : hipL, kn = right ? knR : knL, an = right ? anR : anL;
    const f32 side = right ? -1.0f : 1.0f;
    const u32 upleg = right ? COD_BONE_UP_LEG_R : COD_BONE_UP_LEG_L, leg = right ? COD_BONE_LEG_R : COD_BONE_LEG_L,
              foot = right ? COD_BONE_FOOT_R : COD_BONE_FOOT_L, toe = right ? COD_BONE_TOE_R : COD_BONE_TOE_L;
    s = cab_new(v, CAB_PART_LEG, CAB_MAT_CLOTH);
    s->p0 = hip;
    s->p1 = kn;
    s->p2 = (v3){an.x, an.y + 0.085f, an.z + 0.008f};
    cab_radii(s, 7, cab_variant_specs_leg_r);
    s->rings = 24;
    s->seg = 17;
    s->fold = 0.0018f;
    s->crease = 0.0042f;
    cab_cand(s, COD_BONE_HIPS, 0.6f); cab_cand(s, upleg, 1.0f); cab_cand(s, leg, 1.0f); cab_cand(s, foot, 0.5f);
    cab_look(s, (v3){0.98f, 0.98f, 0.97f}, 0.8f, 0.72f, 0.22f, 0.10f);

    s = cab_new(v, CAB_PART_POUCH, CAB_MAT_CLOTH);
    s->pouch = cab_pouch_of(0.052f, 0.070f, 0.026f, hip.x + side * 0.062f, hip.y - 0.16f, 0.026f);
    s->pouch.rz = side * 0.06f;
    s->pouch.ry = side * 0.55f;
    s->pouch.bend = 0.11f;
    cab_cand(s, upleg, 1.0f); cab_cand(s, COD_BONE_HIPS, 0.4f);
    cab_look(s, (v3){0.95f, 0.95f, 0.94f}, 0.9f, 0.5f, 0.4f, 0.16f);

    if (V->knee_pads) {
      s = cab_new(v, CAB_PART_KNEE_PAD, CAB_MAT_GEAR);
      s->p0 = kn;
      s->side = side;
      cab_cand(s, leg, 1.0f); cab_cand(s, upleg, 0.5f);
      cab_look(s, CAB_G_PAD, 1.0f, 0.9f, 0.28f, 0.20f);
    }

    s = cab_new(v, CAB_PART_BOOT, CAB_MAT_BOOT);
    s->p0 = an;
    s->side = side;
    cab_cand(s, leg, 0.55f); cab_cand(s, foot, 1.0f); cab_cand(s, toe, 0.6f);
    cab_look(s, CAB_G_BOOT, 0.9f, 0.85f, 0.5f, 0.3f);

    s = cab_new(v, CAB_PART_BOOT_SOLE, CAB_MAT_RUBBER);
    s->p0 = an;
    cab_cand(s, foot, 1.0f); cab_cand(s, toe, 0.8f);
    cab_look(s, (v3){1, 1, 1}, 0.9f, 1.0f, -1.0f, 0.0f);

    s = cab_new(v, CAB_PART_BOOT_LACES, CAB_MAT_BOOT);
    s->p0 = an;
    s->bone = (i32)foot;
    cab_look(s, CAB_G_LACE, 0.9f, 0.85f, -1.0f, 0.0f);
  }

  // load-bearing gear
  s = cab_new(v, CAB_PART_CARRIER, CAB_MAT_PLATE);
  cab_cand(s, COD_BONE_SPINE, 0.7f); cab_cand(s, COD_BONE_SPINE1, 1.0f); cab_cand(s, COD_BONE_SPINE2, 1.0f); cab_cand(s, COD_BONE_CLAVICLE_R, 0.45f); cab_cand(s, COD_BONE_CLAVICLE_L, 0.45f);
  cab_look(s, (v3){0.72f, 0.72f, 0.72f}, 0.85f, 0.3f, 0.30f, 0.18f);

  s = cab_new(v, CAB_PART_WEBBING, CAB_MAT_GEAR);
  cab_cand(s, COD_BONE_SPINE1, 1.0f); cab_cand(s, COD_BONE_SPINE2, 1.0f);
  cab_look(s, CAB_G_WEBBING, 1.0f, -1.0f, 0.3f, 0.26f);

  // magazine pouches, deliberately not evenly loaded; the rng draws stay in order
  const u32 n_pouch = V->full_carrier ? 3 : 2;
  for (u32 i = 0; i < n_pouch; i++) {
    f32 t = n_pouch == 1 ? 0.5f : (f32)i / (f32)(n_pouch - 1);
    f32 x = (t - 0.5f) * (n_pouch > 2 ? 0.156f : 0.09f);
    s = cab_new(v, CAB_PART_POUCH, CAB_MAT_GEAR);
    s->pouch = cab_pouch_of(0.033f, 0.056f, 0.034f, x, 1.236f + v->pouch_y[i], 0.148f);
    s->pouch.rx = -0.10f;
    s->pouch.rz = v->pouch_rz[i];
    s->pouch.lid_tilt = i == 1 ? -0.5f : 0.0f;
    s->pouch.bend = 0.26f;
    cab_cand(s, COD_BONE_SPINE1, 1.0f); cab_cand(s, COD_BONE_SPINE2, 0.8f); cab_cand(s, COD_BONE_SPINE, 0.4f);
    cab_look(s, i == 1 ? CAB_G_POUCH_ALT : CAB_G_POUCH, 0.9f, 0.25f, 0.5f, 0.30f);
    if (i == 1) {
      s = cab_new(v, CAB_PART_POUCH, CAB_MAT_POLYMER);
      s->pouch = cab_pouch_of(0.0145f, 0.042f, 0.023f, x, 1.308f, 0.152f);
      s->pouch.rx = -0.12f;
      cab_cand(s, COD_BONE_SPINE1, 1.0f); cab_cand(s, COD_BONE_SPINE2, 0.8f);
      cab_look(s, (v3){1, 1, 1}, 0.4f, -1.0f, -1.0f, 0.2f);
    }
  }

  // radio, antenna, belt, dump pouch
  s = cab_new(v, CAB_PART_POUCH, CAB_MAT_GEAR);
  s->pouch = cab_pouch_of(0.032f, 0.058f, 0.028f, 0.112f, 1.336f, 0.118f);
  s->pouch.ry = 0.35f;
  s->pouch.rz = 0.10f;
  s->pouch.bend = 0.24f;
  cab_cand(s, COD_BONE_SPINE2, 1.0f); cab_cand(s, COD_BONE_CLAVICLE_L, 0.5f); cab_cand(s, COD_BONE_SPINE1, 0.5f);
  cab_look(s, CAB_G_POUCH_ALT, 0.9f, -1.0f, 0.5f, 0.22f);

  s = cab_new(v, CAB_PART_POUCH, CAB_MAT_POLYMER);
  s->pouch = cab_pouch_of(0.005f, 0.075f, 0.005f, 0.116f, 1.424f, 0.104f);
  s->pouch.rx = -0.18f;
  cab_cand(s, COD_BONE_SPINE2, 1.0f); cab_cand(s, COD_BONE_CLAVICLE_L, 0.6f);
  cab_look(s, (v3){1, 1, 1}, 0.3f, -1.0f, -1.0f, 0.0f);

  s = cab_new(v, CAB_PART_BELT, CAB_MAT_GEAR);
  cab_cand(s, COD_BONE_HIPS, 1.0f); cab_cand(s, COD_BONE_SPINE, 0.5f);
  cab_look(s, CAB_G_BELT, 1.0f, 0.4f, 0.25f, 0.26f);

  s = cab_new(v, CAB_PART_POUCH, CAB_MAT_GEAR);
  s->pouch = cab_pouch_of(0.048f, 0.062f, 0.038f, -0.142f, 0.878f, -0.070f);
  s->pouch.rz = -0.12f;
  s->pouch.ry = -0.5f;
  cab_cand(s, COD_BONE_HIPS, 1.0f); cab_cand(s, COD_BONE_SPINE, 0.4f);
  cab_look(s, CAB_G_DUMP, 0.95f, 0.6f, 0.45f, 0.26f);

  // head
  s = cab_new(v, CAB_PART_HEAD, CAB_MAT_SKIN);
  s->p0 = head;
  s->bone = COD_BONE_HEAD;
  cab_look(s, (v3){1, 1, 1}, 0.3f, 0.06f, -1.0f, 0.0f);
  s = cab_new(v, CAB_PART_NOSE, CAB_MAT_SKIN);
  s->p0 = head;
  s->bone = COD_BONE_HEAD;
  cab_look(s, (v3){1, 1, 1}, 0.25f, -1.0f, -1.0f, 0.0f);
  for (u32 e = 0; e < 2; e++) {
    s = cab_new(v, CAB_PART_EAR, CAB_MAT_SKIN);
    s->p0 = head;
    s->side = e == 0 ? -1.0f : 1.0f;
    s->bone = COD_BONE_HEAD;
    cab_look(s, (v3){1, 1, 1}, 0.45f, -1.0f, -1.0f, 0.0f);
  }
  for (u32 e = 0; e < 2; e++) {
    s = cab_new(v, CAB_PART_EYE, CAB_MAT_POLYMER);
    s->p0 = head;
    s->side = e == 0 ? -1.0f : 1.0f;
    s->bone = COD_BONE_HEAD;
    cab_look(s, (v3){0.55f, 0.5f, 0.45f}, 0.2f, -1.0f, -1.0f, 0.0f);
  }
  s = cab_new(v, CAB_PART_NECK, CAB_MAT_SKIN);
  s->p0 = head;
  cab_cand(s, COD_BONE_NECK, 1.0f); cab_cand(s, COD_BONE_HEAD, 0.7f); cab_cand(s, COD_BONE_SPINE2, 0.4f);
  cab_look(s, (v3){1, 1, 1}, 0.5f, -1.0f, -1.0f, 0.0f);

  if (V->face_wrap) {
    const b32 hard = V->mask_hard;
    s = cab_new(v, CAB_PART_FACE_WRAP, hard ? CAB_MAT_POLYMER : CAB_MAT_GEAR);
    s->p0 = head;
    cab_cand(s, COD_BONE_HEAD, 1.0f); cab_cand(s, COD_BONE_NECK, 0.5f);
    cab_look(s, hard ? CAB_G_MASK : V->helmet ? CAB_G_WRAP : (v3){0.78f, 0.74f, 0.66f}, hard ? 0.5f : 0.85f, hard ? 0.1f : 0.2f, hard ? 0.2f : 0.3f,
             hard ? 0.3f : 0.1f);
  }

  if (V->helmet) {
    s = cab_new(v, CAB_PART_HELMET, V->helmet_cover ? CAB_MAT_CLOTH : CAB_MAT_PLATE);
    s->p0 = head;
    s->bone = COD_BONE_HEAD;
    cab_look(s, V->helmet_tint, 0.6f, 0.2f, 0.55f, 0.4f);
    s = cab_new(v, CAB_PART_HELMET_HW, CAB_MAT_POLYMER);
    s->p0 = head;
    s->bone = COD_BONE_HEAD;
    cab_look(s, (v3){1, 1, 1}, 0.5f, -1.0f, -1.0f, 0.3f);
    s = cab_new(v, CAB_PART_CHIN_STRAP, CAB_MAT_GEAR);
    s->p0 = head;
    cab_cand(s, COD_BONE_HEAD, 1.0f); cab_cand(s, COD_BONE_NECK, 0.4f);
    cab_look(s, CAB_G_STRAP, 0.95f, -1.0f, 0.25f, 0.18f);
    if (V->goggles) {
      s = cab_new(v, CAB_PART_GOGGLE_FRAME, CAB_MAT_POLYMER);
      s->p0 = head;
      s->flag = V->goggles_down;
      s->bone = COD_BONE_HEAD;
      cab_look(s, (v3){1, 1, 1}, 0.4f, -1.0f, -1.0f, 0.35f);
      s = cab_new(v, CAB_PART_GOGGLE_STRAP, CAB_MAT_GEAR);
      s->p0 = head;
      s->flag = V->goggles_down;
      s->bone = COD_BONE_HEAD;
      cab_look(s, CAB_G_STRAP, 0.85f, -1.0f, 0.3f, 0.0f);
      s = cab_new(v, CAB_PART_GOGGLE_LENS, CAB_MAT_GLASS);
      s->p0 = head;
      s->flag = V->goggles_down;
      s->bone = COD_BONE_HEAD;
      cab_look(s, (v3){1, 1, 1}, 0.15f, -1.0f, -1.0f, 0.0f);
    }
  } else if (V->head_wrap) {
    s = cab_new(v, CAB_PART_SCARF, CAB_MAT_CLOTH);
    s->p0 = head;
    s->bone = COD_BONE_HEAD;
    cab_look(s, (v3){0.82f, 0.80f, 0.76f}, 0.8f, 0.25f, 0.4f, 0.14f);
  }

  if (V->shades) {
    s = cab_new(v, CAB_PART_SHADE_FRAME, CAB_MAT_POLYMER);
    s->p0 = head;
    s->bone = COD_BONE_HEAD;
    cab_look(s, (v3){1, 1, 1}, 0.4f, -1.0f, -1.0f, 0.3f);
    s = cab_new(v, CAB_PART_SHADE_LENS, CAB_MAT_GLASS);
    s->p0 = head;
    s->bone = COD_BONE_HEAD;
    cab_look(s, (v3){1, 1, 1}, 0.2f, -1.0f, -1.0f, 0.0f);
  }

  // hands and weapon
  const v3 bore = v3_normalize((v3){0.115f, -0.10f, 1.0f});
  const v3 palm_r = v3_normalize((v3){-0.55f, 0.35f, -0.75f});
  const v3 palm_l = v3_normalize((v3){0.75f, 0.30f, -0.60f});
  const v3 grip_r = v3_normalize((v3){0.18f, 0.92f, -0.34f});
  const v3 grip_l = bore;
  s = cab_new(v, CAB_PART_GLOVE, CAB_MAT_BOOT);
  s->p0 = wrR;
  s->p1 = grip_r;
  s->p2 = palm_r;
  s->side = -1.0f;
  cab_cand(s, COD_BONE_HAND_R, 1.0f); cab_cand(s, COD_BONE_FOREARM_R, 0.35f);
  cab_look(s, CAB_G_GLOVE, 0.9f, 0.3f, 0.25f, 0.28f);
  s = cab_new(v, CAB_PART_KNUCKLE, CAB_MAT_POLYMER);
  s->p0 = wrR;
  s->p1 = grip_r;
  s->p2 = palm_r;
  s->bone = COD_BONE_HAND_R;
  cab_look(s, (v3){1, 1, 1}, 0.5f, -1.0f, -1.0f, 0.35f);
  s = cab_new(v, CAB_PART_GLOVE, CAB_MAT_BOOT);
  s->p0 = wrL;
  s->p1 = grip_l;
  s->p2 = palm_l;
  s->side = 1.0f;
  cab_cand(s, COD_BONE_HAND_L, 1.0f); cab_cand(s, COD_BONE_FOREARM_L, 0.35f);
  cab_look(s, CAB_G_GLOVE, 0.9f, 0.3f, 0.25f, 0.28f);
  s = cab_new(v, CAB_PART_KNUCKLE, CAB_MAT_POLYMER);
  s->p0 = wrL;
  s->p1 = grip_l;
  s->p2 = palm_l;
  s->bone = COD_BONE_HAND_L;
  cab_look(s, (v3){1, 1, 1}, 0.5f, -1.0f, -1.0f, 0.35f);

  // the weapon job builds all four meshes; its pieces below borrow them
  s = cab_new(v, CAB_PART_WEAPON, CAB_MAT_STEEL);
  s->job = true;
  s->mesh = NULL;
  s->bone = -2; // the build only, never merged itself
  s = cab_new(v, CAB_PART_WPN_STEEL, CAB_MAT_STEEL);
  s->mesh = &v->weapon.steel;
  s->bone = COD_BONE_HAND_R;
  cab_look(s, (v3){1, 1, 1}, 0.55f, -1.0f, -1.0f, 0.25f);
  s = cab_new(v, CAB_PART_WPN_POLY, CAB_MAT_POLYMER);
  s->mesh = &v->weapon.polymer;
  s->bone = COD_BONE_HAND_R;
  cab_look(s, (v3){1, 1, 1}, 0.5f, -1.0f, -1.0f, 0.3f);
  s = cab_new(v, CAB_PART_WPN_RUBBER, CAB_MAT_RUBBER);
  s->mesh = &v->weapon.rubber;
  s->bone = COD_BONE_HAND_R;
  cab_look(s, (v3){1, 1, 1}, 0.6f, -1.0f, -1.0f, 0.0f);
  if (!V->long_weapon) {
    s = cab_new(v, CAB_PART_WPN_GLASS, CAB_MAT_GLASS);
    s->mesh = &v->weapon.glass;
    s->bone = COD_BONE_HAND_R;
    cab_look(s, (v3){1, 1, 1}, 0.1f, -1.0f, -1.0f, 0.0f);
  }

  // sling: body-bound so it stays on the chest as the arms move
  s = cab_new(v, CAB_PART_SLING, CAB_MAT_GEAR);
  cab_cand(s, COD_BONE_SPINE2, 1.0f); cab_cand(s, COD_BONE_SPINE1, 0.8f); cab_cand(s, COD_BONE_CLAVICLE_R, 0.5f); cab_cand(s, COD_BONE_CLAVICLE_L, 0.5f); cab_cand(s, COD_BONE_HIPS, 0.3f);
  cab_look(s, CAB_G_WEBBING, 1.0f, -1.0f, 0.2f, 0.22f);

  // the build groups the parts by material in first-met order
  u32 n = 0;
  for (u32 m = 0; m < CAB_MAT_COUNT; m++)
    for (u32 i = 0; i < v->spec_count; i++)
      if (v->spec[i].mat == m && v->spec[i].bone != -2)
        v->order[n++] = i;
  v->order_count = n;
  cab_weapon_anchors(&v->weapon, V->long_weapon, v->cant);
}

// ---------------------------------------------------------------- parts ---

// one part's mesh in the actor's bind space, normals computed and welded like B.add
void cab_part_build(const CabNoise *nz, const CabVariant *v, const CabSpec *s, PgMesh *out, ArenaAllocator *arena) {
  (void)arena;
  switch (s->kind) {
  case CAB_PART_JACKET: cab_jacket(nz, out, s->scalar); break;
  case CAB_PART_PELVIS: cab_pelvis(nz, out); break;
  case CAB_PART_COLLAR: cab_collar(nz, out); break;
  case CAB_PART_SHOULDER_CAP: cab_shoulder_cap(nz, out, s->p0, s->side); break;
  case CAB_PART_SLEEVE:
  case CAB_PART_ELBOW_PAD:
  case CAB_PART_LEG: cab_limb_tube(nz, out, s->p0, s->p1, s->p2, s->radii, s->radii_count, s->rings, s->seg, s->fold, s->crease, s->bend); break;
  case CAB_PART_POUCH: cab_pouch(nz, out, &s->pouch); break;
  case CAB_PART_KNEE_PAD: cab_knee_pad(nz, out, s->p0); break;
  case CAB_PART_BOOT: cab_boot(nz, out, s->p0); break;
  case CAB_PART_BOOT_SOLE: cab_boot_sole(out, s->p0); break;
  case CAB_PART_BOOT_LACES: cab_boot_laces(out, s->p0); break;
  case CAB_PART_CARRIER: cab_carrier(nz, out); break;
  case CAB_PART_WEBBING: cab_webbing(out); break;
  case CAB_PART_BELT: cab_belt(nz, out); break;
  case CAB_PART_HEAD: cab_head(nz, out, s->p0); break;
  case CAB_PART_NOSE: cab_nose(out, s->p0); break;
  case CAB_PART_EAR: cab_ear(out, s->p0, s->side); break;
  case CAB_PART_EYE: cab_eyeball(out, s->p0, s->side); break;
  case CAB_PART_NECK: cab_neck(nz, out, s->p0); break;
  case CAB_PART_FACE_WRAP: cab_face_wrap(nz, out, s->p0); break;
  case CAB_PART_HELMET: cab_helmet(nz, out, s->p0); break;
  case CAB_PART_HELMET_HW: cab_helmet_hw(nz, out, s->p0); break;
  case CAB_PART_CHIN_STRAP: cab_chin_strap(out, s->p0); break;
  case CAB_PART_GOGGLE_FRAME: cab_goggle_frame(out, s->p0, s->flag); break;
  case CAB_PART_GOGGLE_STRAP: cab_goggle_strap(out, s->p0, s->flag); break;
  case CAB_PART_GOGGLE_LENS: cab_goggle_lens(out, s->p0, s->flag); break;
  case CAB_PART_SCARF: cab_scarf(nz, out, s->p0); break;
  case CAB_PART_SHADE_FRAME: {
    PgMesh *lens = cab_mesh(out->arena);
    cab_sunglasses(lens, out, s->p0);
    break;
  }
  case CAB_PART_SHADE_LENS: {
    PgMesh *frame = cab_mesh(out->arena);
    cab_sunglasses(out, frame, s->p0);
    break;
  }
  case CAB_PART_GLOVE: cab_glove(nz, out, s->p0, s->p1, s->p2, s->side); break;
  case CAB_PART_KNUCKLE: cab_knuckle(out, s->p0, s->p1, s->p2); break;
  case CAB_PART_SLING: cab_sling(out, v->weapon.foregrip, v->weapon.stock_top); break;
  default: break;
  }
  cab_compute_normals(out);
  cab_weld_normals(out);
}
