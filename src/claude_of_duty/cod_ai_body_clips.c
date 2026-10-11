// Claude of Duty soldier bodies: the animation content. Poses are local euler deltas in degrees on the bind pose.

typedef CabPoser Poser;

#define CAB_TAU (2.0f * (f32)M_PI)

// additive euler delta in degrees on a bone, scaled by the layer weight
hz_internal void cab_pd(Poser *P, u32 bone, f32 x, f32 y, f32 z) {
  P->d[bone] = (v3){P->d[bone].x + x * P->w, P->d[bone].y + y * P->w, P->d[bone].z + z * P->w};
}

hz_internal void cab_phip(Poser *P, f32 x, f32 y, f32 z) { P->hip = (v3){P->hip.x + x * P->w, P->hip.y + y * P->w, P->hip.z + z * P->w}; }

// smooth positive lobe used for knee and ankle curves
hz_internal f32 cab_lobe(f32 x, f32 k) {
  f32 s = m_sinf(x);
  return s > 0.0f ? m_powf(s, k) : 0.0f;
}

#define D(bone, x, y, z) cab_pd(P, bone, x, y, z)
#define HIP(x, y, z) cab_phip(P, x, y, z)

// weight on the left leg, knees soft, weapon at low ready
hz_internal void cab_clip_idle(Poser *P, f32 ph) {
  f32 t = ph * CAB_TAU;
  f32 breath = m_sinf(t * 0.55f);
  f32 sway = m_sinf(t * 0.31f + 1.1f);
  f32 micro = m_sinf(t * 1.7f + 0.4f) * 0.35f + m_sinf(t * 2.9f) * 0.2f;
  HIP(0.012f * sway, -0.008f + 0.004f * breath, 0.0f);
  D(COD_BONE_HIPS, -1.5f, 2.2f * sway, 1.6f);
  D(COD_BONE_SPINE, 1.6f + 0.7f * breath, -1.4f * sway, -0.8f);
  D(COD_BONE_SPINE1, 1.2f + 0.9f * breath, -1.0f * sway, -0.6f);
  D(COD_BONE_SPINE2, -0.6f + 1.1f * breath, 1.6f * sway, 0.4f);
  D(COD_BONE_NECK, 1.0f - 0.5f * breath, 1.2f * sway + micro, 0.0f);
  D(COD_BONE_HEAD, -1.2f, 1.0f * micro, 0.6f * sway);
  D(COD_BONE_UP_LEG_R, -2.0f, 1.5f, -1.5f);
  D(COD_BONE_LEG_R, -5.5f, 0.0f, 0.0f);
  D(COD_BONE_FOOT_R, 4.5f, -1.5f, 0.0f);
  D(COD_BONE_UP_LEG_L, 5.0f, -4.5f, 2.5f);
  D(COD_BONE_LEG_L, -9.0f, 0.0f, 0.0f);
  D(COD_BONE_FOOT_L, 5.5f, 3.0f, 0.0f);
  D(COD_BONE_CLAVICLE_R, -1.5f + 0.8f * breath, 0.0f, 1.2f);
  D(COD_BONE_CLAVICLE_L, -1.0f + 0.6f * breath, 0.0f, -1.0f);
  D(COD_BONE_UPPER_ARM_R, -3.0f, 0.0f, 2.0f);
  D(COD_BONE_UPPER_ARM_L, 2.0f, 0.0f, -2.0f);
  D(COD_BONE_FOREARM_R, 2.0f, 0.0f, 0.0f);
}

// stock in the shoulder, head over the sights, weight forward on bent knees
hz_internal void cab_clip_aim_add(Poser *P, f32 w) {
  HIP(0.0f, -0.035f * w, 0.012f * w);
  D(COD_BONE_HIPS, 4 * w, 3 * w, 0.0f);
  D(COD_BONE_UP_LEG_R, 8 * w, 4 * w, -3 * w);
  D(COD_BONE_LEG_R, -17 * w, 0.0f, 0.0f);
  D(COD_BONE_FOOT_R, 9 * w, -2 * w, 0.0f);
  D(COD_BONE_UP_LEG_L, 3 * w, -6 * w, 4 * w);
  D(COD_BONE_LEG_L, -13 * w, 0.0f, 0.0f);
  D(COD_BONE_FOOT_L, 8 * w, 3 * w, 0.0f);
  D(COD_BONE_SPINE1, 2.5f * w, 0.0f, 0.0f);
  D(COD_BONE_SPINE2, 3.0f * w, -5.0f * w, 0.0f);
  D(COD_BONE_NECK, 5.0f * w, 3.0f * w, 0.0f);
  D(COD_BONE_HEAD, -3.5f * w, 2.0f * w, -1.5f * w);
  D(COD_BONE_CLAVICLE_R, -6.0f * w, -2 * w, 5.0f * w);
  D(COD_BONE_CLAVICLE_L, -3.0f * w, 4 * w, -3.0f * w);
  D(COD_BONE_UPPER_ARM_R, 10 * w, 0.0f, 14 * w);
  D(COD_BONE_FOREARM_R, -12 * w, 0.0f, 0.0f);
  D(COD_BONE_UPPER_ARM_L, 8 * w, 0.0f, -6 * w);
}

typedef struct CabGait {
  f32 thigh, thigh_bias, thigh_twist, splay;
  f32 knee_base, knee, knee_stance;
  f32 ankle, ankle_bias, toe;
  f32 sway, bob, bob_bias;
  f32 pelvis_tilt, pelvis_yaw, pelvis_roll;
  f32 lean, spine_yaw, arm_swing;
} CabGait;

hz_internal const CabGait cab_gait_walk = {21, -2, 1.5f, 1.5f, 7, 46, 8, 12, 2, 16, 0.014f, 0.014f, -0.014f, -1, 4.5f, 3.2f, 4, 3.4f, 3.5f};
hz_internal const CabGait cab_gait_run = {34, 2, 2, 2, 14, 86, 22, 20, 4, 26, 0.02f, 0.03f, -0.03f, -3, 7, 5, 13, 6, 7};
hz_internal const CabGait cab_gait_crouch = {13, 38, 2, 4, 74, 26, 6, 8, 26, 8, 0.01f, 0.008f, -0.008f, 6, 3, 2, 16, 2.4f, 2};

hz_internal void cab_gait(Poser *P, f32 ph, const CabGait *k) {
  f32 t = ph * CAB_TAU;
  for (u32 s = 0; s < 2; s++) {
    f32 side = s == 0 ? 1.0f : -1.0f;
    u32 upleg = s == 0 ? COD_BONE_UP_LEG_R : COD_BONE_UP_LEG_L, leg = s == 0 ? COD_BONE_LEG_R : COD_BONE_LEG_L,
        foot = s == 0 ? COD_BONE_FOOT_R : COD_BONE_FOOT_L, toe = s == 0 ? COD_BONE_TOE_R : COD_BONE_TOE_L;
    f32 o = s == 0 ? 0.0f : (f32)M_PI;
    f32 a = t + o;
    f32 thigh = k->thigh * m_sinf(a) + k->thigh_bias;
    f32 knee = -(k->knee_base + k->knee * cab_lobe(a - 0.55f, 1.5f) + k->knee_stance * cab_lobe(a + (f32)M_PI + 0.4f, 2.0f));
    f32 ankle = k->ankle * m_sinf(a - 1.9f) + k->ankle_bias;
    D(upleg, thigh, side * k->thigh_twist, side * k->splay);
    D(leg, knee, 0.0f, 0.0f);
    D(foot, ankle, -side * 1.5f, 0.0f);
    D(toe, m_maxf(0.0f, -k->toe * m_sinf(a - 2.6f)), 0.0f, 0.0f);
  }
  HIP(k->sway * m_sinf(t), k->bob_bias + k->bob * m_cosf(2.0f * t), 0.0f);
  D(COD_BONE_HIPS, k->pelvis_tilt, k->pelvis_yaw * m_sinf(t), k->pelvis_roll * m_sinf(t + 1.2f));
  D(COD_BONE_SPINE, k->lean * 0.35f, -k->spine_yaw * 0.45f * m_sinf(t), -k->pelvis_roll * 0.35f * m_sinf(t + 1.2f));
  D(COD_BONE_SPINE1, k->lean * 0.35f, -k->spine_yaw * 0.75f * m_sinf(t), 0.0f);
  D(COD_BONE_SPINE2, k->lean * 0.3f, -k->spine_yaw * m_sinf(t), 0.0f);
  D(COD_BONE_NECK, -k->lean * 0.5f, k->spine_yaw * 0.6f * m_sinf(t), 0.0f);
  D(COD_BONE_CLAVICLE_R, -k->arm_swing * m_sinf(t) - 1.0f, 0.0f, 1.5f);
  D(COD_BONE_CLAVICLE_L, k->arm_swing * m_sinf(t) - 1.0f, 0.0f, -1.5f);
  D(COD_BONE_UPPER_ARM_R, -k->arm_swing * 0.6f * m_sinf(t), 0.0f, 2.0f);
  D(COD_BONE_UPPER_ARM_L, k->arm_swing * 0.8f * m_sinf(t), 0.0f, -2.0f);
}

hz_internal void cab_clip_walk(Poser *P, f32 ph) { cab_gait(P, ph, &cab_gait_walk); }

hz_internal void cab_clip_run(Poser *P, f32 ph) {
  cab_gait(P, ph, &cab_gait_run);
  D(COD_BONE_HEAD, -3.0f, 0.0f, 0.0f);
}

hz_internal void cab_clip_crouch_walk(Poser *P, f32 ph) {
  cab_gait(P, ph, &cab_gait_crouch);
  HIP(0.0f, -0.30f, -0.02f);
  D(COD_BONE_SPINE2, 4.0f, 0.0f, 0.0f);
}

// static crouch: knees loaded, torso upright behind the weapon
hz_internal void cab_clip_crouch_idle(Poser *P, f32 ph) {
  f32 t = ph * CAB_TAU;
  f32 breath = m_sinf(t * 0.6f);
  HIP(0.004f * m_sinf(t * 0.4f), -0.315f + 0.004f * breath, -0.02f);
  D(COD_BONE_HIPS, 7.0f, 1.5f, 1.0f);
  D(COD_BONE_UP_LEG_R, 44.0f, 3.0f, -6.0f);
  D(COD_BONE_LEG_R, -78.0f, 0.0f, 0.0f);
  D(COD_BONE_FOOT_R, 30.0f, -2.0f, 0.0f);
  D(COD_BONE_UP_LEG_L, 36.0f, -6.0f, 7.0f);
  D(COD_BONE_LEG_L, -86.0f, 0.0f, 0.0f);
  D(COD_BONE_FOOT_L, 32.0f, 4.0f, 0.0f);
  D(COD_BONE_SPINE, 6.0f + 0.6f * breath, 0.0f, 0.0f);
  D(COD_BONE_SPINE1, 5.0f + 0.8f * breath, 0.0f, 0.0f);
  D(COD_BONE_SPINE2, 3.0f + 1.0f * breath, 0.0f, 0.0f);
  D(COD_BONE_NECK, 2.0f, 0.0f, 0.0f);
  D(COD_BONE_CLAVICLE_R, -2.0f, 0.0f, 1.5f);
  D(COD_BONE_CLAVICLE_L, -1.5f, 0.0f, -1.5f);
}

// a wounded low stance
hz_internal void cab_clip_hurt_idle(Poser *P, f32 ph) {
  f32 t = ph * CAB_TAU;
  HIP(0.0f, -0.10f, -0.03f);
  D(COD_BONE_HIPS, 10.0f, 0.0f, 4.0f);
  D(COD_BONE_SPINE, 12.0f, 0.0f, -3.0f);
  D(COD_BONE_SPINE1, 9.0f, 0.0f, -2.0f);
  D(COD_BONE_SPINE2, 5.0f + m_sinf(t * 1.6f), 0.0f, 0.0f);
  D(COD_BONE_NECK, 6.0f, 0.0f, 0.0f);
  D(COD_BONE_UP_LEG_R, 16.0f, 0.0f, -3.0f);
  D(COD_BONE_LEG_R, -28.0f, 0.0f, 0.0f);
  D(COD_BONE_FOOT_R, 12.0f, 0.0f, 0.0f);
  D(COD_BONE_UP_LEG_L, 10.0f, 0.0f, 4.0f);
  D(COD_BONE_LEG_L, -20.0f, 0.0f, 0.0f);
  D(COD_BONE_FOOT_L, 9.0f, 0.0f, 0.0f);
}

// pivot on the balls of the feet: the trailing foot lifts and re-plants (t is 0..1 over the clip)
hz_internal void cab_clip_turn_step(Poser *P, f32 t, f32 dir) {
  f32 e = m_sinf((f32)M_PI * m_minf(1.0f, t));
  u32 s_up = dir > 0.0f ? COD_BONE_UP_LEG_R : COD_BONE_UP_LEG_L, s_leg = dir > 0.0f ? COD_BONE_LEG_R : COD_BONE_LEG_L,
      s_foot = dir > 0.0f ? COD_BONE_FOOT_R : COD_BONE_FOOT_L;
  u32 o_up = dir > 0.0f ? COD_BONE_UP_LEG_L : COD_BONE_UP_LEG_R, o_leg = dir > 0.0f ? COD_BONE_LEG_L : COD_BONE_LEG_R;
  D(s_up, 12 * e, dir * 16 * e, 0.0f);
  D(s_leg, -34 * e, 0.0f, 0.0f);
  D(s_foot, 16 * e, 0.0f, 0.0f);
  D(o_up, -4 * e, -dir * 4 * e, 0.0f);
  D(o_leg, -10 * e, 0.0f, 0.0f);
  D(COD_BONE_HIPS, 0.0f, dir * 6 * e, dir * -2 * e);
  HIP(0.0f, -0.012f * e, 0.0f);
}

// vault: plant the support hand, tuck the knees over the obstacle, land (the translation is driven by the brain)
hz_internal void cab_clip_vault(Poser *P, f32 t) {
  f32 rise = m_sinf((f32)M_PI * m_minf(1.0f, t * 1.05f));
  f32 tuck = m_sinf((f32)M_PI * m_minf(1.0f, m_maxf(0.0f, (t - 0.12f) * 1.3f)));
  f32 land = m_maxf(0.0f, (t - 0.7f) / 0.3f);
  HIP(0.0f, 0.10f * rise, 0.02f * rise);
  D(COD_BONE_HIPS, 26 * rise - 16 * land, 0.0f, 0.0f);
  D(COD_BONE_SPINE, 20 * rise, 0.0f, -4 * rise);
  D(COD_BONE_SPINE1, 14 * rise, 0.0f, -3 * rise);
  D(COD_BONE_SPINE2, 8 * rise, -14 * rise, 0.0f);
  D(COD_BONE_NECK, -8 * rise, 6 * rise, 0.0f);
  D(COD_BONE_UP_LEG_R, 86 * tuck + 30 * land, 0.0f, -10 * tuck);
  D(COD_BONE_LEG_R, -104 * tuck - 20 * land, 0.0f, 0.0f);
  D(COD_BONE_FOOT_R, 24 * tuck, 0.0f, 0.0f);
  D(COD_BONE_UP_LEG_L, 68 * tuck + 12 * land, 0.0f, 12 * tuck);
  D(COD_BONE_LEG_L, -92 * tuck - 30 * land, 0.0f, 0.0f);
  D(COD_BONE_FOOT_L, 20 * tuck, 0.0f, 0.0f);
  D(COD_BONE_CLAVICLE_L, -18 * rise, 12 * rise, -14 * rise);
  D(COD_BONE_UPPER_ARM_L, -46 * rise, 0.0f, -28 * rise);
  D(COD_BONE_FOREARM_L, -30 * rise, 0.0f, 0.0f);
  D(COD_BONE_CLAVICLE_R, -6 * rise, 0.0f, 4 * rise);
  D(COD_BONE_UPPER_ARM_R, -14 * rise, 0.0f, 10 * rise);
}

// firing impulse: a fast spike and a springy settle; t is seconds since the shot
hz_internal void cab_clip_recoil_add(Poser *P, f32 t, f32 strength) {
  if (t > 0.26f)
    return;
  f32 e = m_expf(-t * 16.0f);
  f32 osc = m_sinf(t * 92.0f);
  f32 k = strength * e;
  D(COD_BONE_CLAVICLE_R, -7 * k, 0.0f, 3 * k);
  D(COD_BONE_UPPER_ARM_R, -9 * k + 2 * osc * k, 0.0f, 5 * k);
  D(COD_BONE_FOREARM_R, 7 * k, 0.0f, 0.0f);
  D(COD_BONE_CLAVICLE_L, -3 * k, 0.0f, -2 * k);
  D(COD_BONE_UPPER_ARM_L, -6 * k, 0.0f, -3 * k);
  D(COD_BONE_SPINE2, -3.5f * k, 1.5f * k * osc, 0.0f);
  D(COD_BONE_SPINE1, -2.0f * k, 0.0f, 0.0f);
  D(COD_BONE_NECK, -2.5f * k, 0.0f, 0.0f);
  D(COD_BONE_HEAD, 1.5f * k, 0.8f * k * osc, 0.0f);
}

typedef enum CabHitRegion {
  CAB_HIT_HEAD = 0,
  CAB_HIT_TORSO,
  CAB_HIT_ARM_R,
  CAB_HIT_ARM_L,
  CAB_HIT_LEG_R,
  CAB_HIT_LEG_L,
} CabHitRegion;

// region-specific hit reaction; t is seconds since impact
hz_internal void cab_clip_hit_add(Poser *P, u32 region, f32 t, f32 dir_side, f32 strength) {
  if (t > 0.5f)
    return;
  f32 e = m_expf(-t * 7.5f) * m_minf(1.0f, t * 22.0f);
  f32 k = strength * e;
  f32 side = dir_side >= 0.0f ? 1.0f : -1.0f;
  switch (region) {
  case CAB_HIT_HEAD:
    D(COD_BONE_NECK, -16 * k, 10 * k * side, 6 * k * side);
    D(COD_BONE_HEAD, -20 * k, 14 * k * side, 8 * k * side);
    D(COD_BONE_SPINE2, -7 * k, 4 * k * side, 0.0f);
    D(COD_BONE_SPINE1, -4 * k, 0.0f, 0.0f);
    break;
  case CAB_HIT_TORSO:
    D(COD_BONE_SPINE, -6 * k, 3 * k * side, 2 * k * side);
    D(COD_BONE_SPINE1, -9 * k, 5 * k * side, 3 * k * side);
    D(COD_BONE_SPINE2, -11 * k, 6 * k * side, 4 * k * side);
    D(COD_BONE_NECK, 6 * k, -3 * k * side, 0.0f);
    D(COD_BONE_HIPS, 4 * k, 0.0f, 0.0f);
    HIP(-0.02f * k * side, -0.02f * k, -0.03f * k);
    break;
  case CAB_HIT_ARM_R:
    D(COD_BONE_CLAVICLE_R, -14 * k, 6 * k, 10 * k);
    D(COD_BONE_UPPER_ARM_R, -22 * k, 0.0f, 14 * k);
    D(COD_BONE_FOREARM_R, 16 * k, 0.0f, 0.0f);
    D(COD_BONE_SPINE2, -5 * k, 6 * k, 0.0f);
    break;
  case CAB_HIT_ARM_L:
    D(COD_BONE_CLAVICLE_L, -14 * k, -6 * k, -10 * k);
    D(COD_BONE_UPPER_ARM_L, -24 * k, 0.0f, -16 * k);
    D(COD_BONE_FOREARM_L, 18 * k, 0.0f, 0.0f);
    D(COD_BONE_SPINE2, -5 * k, -6 * k, 0.0f);
    break;
  case CAB_HIT_LEG_R:
    D(COD_BONE_UP_LEG_R, 14 * k, 0.0f, -8 * k);
    D(COD_BONE_LEG_R, -30 * k, 0.0f, 0.0f);
    D(COD_BONE_HIPS, 8 * k, 0.0f, -6 * k);
    HIP(0.0f, -0.05f * k, 0.0f);
    break;
  case CAB_HIT_LEG_L:
    D(COD_BONE_UP_LEG_L, 14 * k, 0.0f, 8 * k);
    D(COD_BONE_LEG_L, -30 * k, 0.0f, 0.0f);
    D(COD_BONE_HIPS, 8 * k, 0.0f, 6 * k);
    HIP(0.0f, -0.05f * k, 0.0f);
    break;
  default:
    D(COD_BONE_SPINE1, -6 * k, 0.0f, 0.0f);
    D(COD_BONE_SPINE2, -6 * k, 0.0f, 0.0f);
    break;
  }
}

// flinch and duck when rounds crack past
hz_internal void cab_clip_suppress_add(Poser *P, f32 w) {
  if (w <= 0.0f)
    return;
  D(COD_BONE_HIPS, 7 * w, 0.0f, 0.0f);
  D(COD_BONE_SPINE, 9 * w, 0.0f, 0.0f);
  D(COD_BONE_SPINE1, 8 * w, 0.0f, 0.0f);
  D(COD_BONE_SPINE2, 6 * w, 0.0f, 0.0f);
  D(COD_BONE_NECK, -6 * w, 0.0f, 0.0f);
  D(COD_BONE_HEAD, -8 * w, 0.0f, 0.0f);
  D(COD_BONE_UP_LEG_R, 16 * w, 0.0f, 0.0f);
  D(COD_BONE_LEG_R, -26 * w, 0.0f, 0.0f);
  D(COD_BONE_UP_LEG_L, 14 * w, 0.0f, 0.0f);
  D(COD_BONE_LEG_L, -24 * w, 0.0f, 0.0f);
  HIP(0.0f, -0.10f * w, 0.0f);
}

// reload body language; the hand path itself is the animator's IK target
hz_internal void cab_clip_reload_add(Poser *P, f32 t) {
  f32 w = m_minf(1.0f, m_maxf(0.0f, m_minf(t * 6.0f, (1.0f - t) * 6.0f)));
  D(COD_BONE_SPINE2, 4 * w, -16 * w, -3 * w);
  D(COD_BONE_SPINE1, 3 * w, -6 * w, 0.0f);
  D(COD_BONE_NECK, 6 * w, 8 * w, 0.0f);
  D(COD_BONE_HEAD, -4 * w, 6 * w, 3 * w);
  D(COD_BONE_CLAVICLE_R, -4 * w, -4 * w, 4 * w);
  D(COD_BONE_UPPER_ARM_R, 6 * w, 0.0f, 10 * w);
  D(COD_BONE_FOREARM_R, -6 * w, 0.0f, 0.0f);
}

hz_internal void cab_clip_play(u32 clip, Poser *P, f32 ph) {
  switch (clip) {
  case COD_CLIP_WALK: cab_clip_walk(P, ph); break;
  case COD_CLIP_RUN: cab_clip_run(P, ph); break;
  case COD_CLIP_CROUCH_WALK: cab_clip_crouch_walk(P, ph); break;
  case COD_CLIP_CROUCH_IDLE: cab_clip_crouch_idle(P, ph); break;
  case COD_CLIP_HURT_IDLE: cab_clip_hurt_idle(P, ph); break;
  default: cab_clip_idle(P, ph); break;
  }
}

#undef D
#undef HIP
