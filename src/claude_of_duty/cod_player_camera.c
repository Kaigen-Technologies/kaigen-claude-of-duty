// Claude of Duty player: the first-person camera feel: eye, bob, springs, rolls, breathing, recoil, shake, fov.

hz_internal CameraShakeTuning cod_shake_tuning(void) {
  return (CameraShakeTuning){.decay = COD_SHAKE_DECAY,
                             .exponent = 2.0f,
                             .frequency = COD_SHAKE_FREQ,
                             .translation = COD_SHAKE_POS,
                             .roll = COD_SHAKE_ROT * 0.7f,
                             .pitch = COD_SHAKE_ROT,
                             .yaw = COD_SHAKE_ROT};
}

hz_internal CodView cod_view_make(EcsEntity camera) {
  CodView v = {.camera = camera,
               .eye = COD_PLAYER_HEIGHT - COD_EYE_OFFSET,
               .dip = {.freq = COD_LAND_FREQ, .damping = COD_LAND_DAMPING},
               .step = {.freq = COD_STEP_FREQ, .damping = COD_STEP_DAMPING},
               .punch = {.freq = COD_RECOIL_PUNCH_FREQ, .damping = COD_RECOIL_PUNCH_DAMPING},
               .recoil_pitch = cod_recoil_make(COD_RECOIL_FREQ, COD_RECOIL_DAMPING, COD_RECOIL_RESIDUAL_TAU, COD_RECOIL_RESIDUAL_SHARE),
               .recoil_yaw = cod_recoil_make(COD_RECOIL_FREQ * 1.08f, COD_RECOIL_DAMPING + 0.06f, COD_RECOIL_RESIDUAL_TAU,
                                             COD_RECOIL_RESIDUAL_SHARE),
               .recoil_roll = cod_recoil_make(COD_RECOIL_FREQ * 0.86f, COD_RECOIL_DAMPING + 0.1f, COD_RECOIL_RESIDUAL_TAU, 0.24f),
               .kick_pitch = cod_recoil_make(11.0f, 0.58f, 0.22f, 0.28f),
               .kick_yaw = cod_recoil_make(11.5f, 0.6f, 0.22f, 0.28f),
               .kick_roll = cod_recoil_make(9.0f, 0.62f, 0.22f, 0.22f),
               .slide_side = 1.0f,
               .fov_move = 1.0f,
               .fov_ads = 1.0f,
               .rot = quat_identity(),
               .fov = COD_FOV};
  camera_shake_init(&v.shake, 0xc0dU);
  return v;
}

// clears every spring, the shake and the smoothed eye
hz_internal void cod_view_reset(CodView *v) {
  EcsEntity camera = v->camera;
  f32 fov = v->fov;
  quat rot = v->rot;
  v3 pos = v->eye_pos;
  b32 placed = v->placed;
  *v = cod_view_make(camera);
  v->fov = fov;
  v->rot = rot;
  v->eye_pos = pos;
  v->placed = placed;
}

// camera-owned recoil: angles in radians, `punch` in metres
hz_internal void cod_view_add_recoil(CodView *v, f32 pitch, f32 yaw, f32 roll, f32 punch) {
  cod_recoil_kick(&v->recoil_pitch, pitch);
  cod_recoil_kick(&v->recoil_yaw, yaw);
  cod_recoil_kick(&v->recoil_roll, roll);
  if (punch != 0.0f)
    v->punch.velocity += -punch * 14.0f;
}

// the weapon's own channel, so the two never fight
hz_internal void cod_view_add_kick(CodView *v, f32 pitch, f32 yaw, f32 roll) {
  cod_recoil_kick(&v->kick_pitch, pitch);
  cod_recoil_kick(&v->kick_yaw, yaw);
  cod_recoil_kick(&v->kick_roll, roll);
}

hz_internal void cod_view_add_trauma(CodView *v, f32 amount) { camera_shake_add(&v->shake, amount); }

// the landing dip, pitch, roll and trauma from the impact speed; returns the strength 0..1
hz_internal f32 cod_view_on_land(CodView *v, f32 speed) {
  f32 t = m_saturatef((speed - COD_LAND_MIN_SPEED) / (COD_LAND_FULL_SPEED - COD_LAND_MIN_SPEED));
  if (t <= 0.0f)
    return 0.0f;
  // a perceptual curve: a 3 m/s landing should still be felt a little
  f32 mag = m_powf(t, 0.72f);
  v->dip.velocity += -COD_LAND_DIP_IMPULSE * mag;
  cod_recoil_kick(&v->recoil_pitch, COD_LAND_PITCH * mag);
  cod_recoil_kick(&v->recoil_roll, COD_LAND_ROLL * mag * v->slide_side);
  cod_view_add_trauma(v, COD_LAND_TRAUMA * mag * mag);
  return mag;
}

hz_internal void cod_view_on_footstep(CodView *v, b32 running, u32 stance) {
  f32 amp = COD_STEP_IMPULSE * (running ? COD_STEP_SPRINT_SCALE : 1.0f);
  if (stance == COD_STANCE_CROUCH)
    amp *= 0.55f;
  else if (stance == COD_STANCE_PRONE)
    amp *= 0.3f;
  v->step.velocity += -amp;
}

hz_internal void cod_view_on_slide_start(CodView *v, f32 side) {
  v->slide_side = side != 0.0f ? side : 1.0f;
  v->dip.velocity += -0.9f;
  cod_view_add_trauma(v, 0.12f);
}

hz_internal f32 cod_slide_progress(const CodMove *m) { return m->sliding ? m_saturatef(m->slide_time / COD_SLIDE_DURATION) : 0.0f; }

// the per-frame composition: `base_fov` is the setting, `ads_fov_scale` the gun's ADS multiplier
hz_internal void cod_view_update(CodView *v, const CodMove *m, const Character *c, f32 health_fraction, f32 suppression, b32 dead,
                                 f32 base_fov, f32 ads_fov_scale, f32 dt, f32 alpha) {
  f32 ads = m_saturatef(m->ads);

  // stance and eye height: crouching is a movement, not a cut
  f32 target_eye = cod_stance_def(m->stance).eye + (m->sliding ? -0.1f : 0.0f);
  f32 eye_tau = (m->stance == COD_STANCE_PRONE || v->eye < 0.75f) ? COD_STANCE_TAU_PRONE
                : target_eye > v->eye                             ? COD_STANCE_TAU_CROUCH_STAND
                                                                  : COD_STANCE_TAU_STAND_CROUCH;
  v->eye = cod_approach(v->eye, target_eye, eye_tau, dt);

  f32 slide_target = m->sliding ? 1.0f - 0.45f * cod_slide_progress(m) : 0.0f;
  v->slide_blend = cod_approach(v->slide_blend, slide_target, m->sliding ? 0.045f : 0.09f, dt);

  // position offsets are built in the yaw basis, so looking up does not turn vertical bob into a lurch
  f32 sy = m_sinf(m->yaw), cy = m_cosf(m->yaw);
  v3 fwd = {-sy, 0.0f, -cy};
  v3 right = {cy, 0.0f, -sy};

  // bob: a 1:2 Lissajous locked to the gait phase, the horizontal extreme exactly on the footfall
  v->bob_phase = m->bob_phase + COD_PI * 0.5f;
  f32 bw = m_minf(COD_BOB_SPEED_CAP, m_powf(m->hspeed / COD_PLAYER_WALK_SPEED, COD_BOB_SPEED_EXP));
  if (!m->grounded || m->sliding)
    bw = 0.0f;
  bw *= m_lerpf(1.0f, COD_BOB_ADS_SCALE, ads);
  if (m->stance == COD_STANCE_PRONE)
    bw *= 0.35f;
  v->bob_weight = cod_approach(v->bob_weight, bw, COD_BOB_AIR_FADE, dt);
  f32 th = v->bob_phase, wt = v->bob_weight;
  v->bob_offset = (v3){m_sinf(th) * COD_BOB_AMP_X * wt, m_sinf(th * 2.0f) * COD_BOB_AMP_Y * wt, m_cosf(th * 2.0f) * COD_BOB_AMP_Z * wt};
  v->bob_roll = -m_sinf(th) * COD_BOB_ROLL * wt;
  v->bob_pitch = m_cosf(th * 2.0f) * COD_BOB_PITCH * wt;

  cod_spring_step(&v->dip, dt);
  cod_spring_step(&v->step, dt);
  cod_spring_step(&v->punch, dt);
  cod_recoil_step(&v->recoil_pitch, dt);
  cod_recoil_step(&v->recoil_yaw, dt);
  cod_recoil_step(&v->recoil_roll, dt);
  cod_recoil_step(&v->kick_pitch, dt);
  cod_recoil_step(&v->kick_yaw, dt);
  cod_recoil_step(&v->kick_roll, dt);

  // a degree of bank into the direction of travel
  f32 strafe_target = -m->cmd.move.x * COD_ROLL_STRAFE * (m->grounded ? 1.0f : 0.45f) * (1.0f - 0.6f * ads);
  v->strafe_roll = cod_approach(v->strafe_roll, strafe_target, COD_ROLL_TAU, dt);
  f32 turn_target = m_clampf(m->yaw_rate * COD_ROLL_YAW_RATE, -COD_ROLL_YAW_RATE_MAX, COD_ROLL_YAW_RATE_MAX) * (1.0f - 0.5f * ads);
  v->turn_roll = cod_approach(v->turn_roll, turn_target, COD_ROLL_TAU * 1.4f, dt);
  v->slide_roll = cod_approach(v->slide_roll, m->sliding ? -v->slide_side * COD_ROLL_SLIDE : 0.0f, 0.1f, dt);
  f32 air_target = m->grounded ? 0.0f : m_clampf(-c->velocity.y * 0.02f, -1.0f, 1.0f) * COD_ROLL_AIR;
  v->air_roll = cod_approach(v->air_roll, air_target, 0.22f, dt);

  // trauma shake: noise driven, decays
  CameraShakeOffset sh = camera_shake_update(&v->shake, cod_shake_tuning(), dt, 1.0f);

  // breathing sway: two detuned sines, amplified by aiming, wounds and suppression
  f32 move_factor = m_saturatef(m->hspeed / 2.2f);
  f32 amp = COD_BREATH_AMP;
  amp *= m_lerpf(1.0f, COD_BREATH_ADS_SCALE, ads);
  amp *= m_lerpf(1.0f, COD_BREATH_LOW_HEALTH_SCALE, 1.0f - m_saturatef(health_fraction));
  amp *= m_lerpf(1.0f, COD_BREATH_SUPPRESSION_SCALE, m_saturatef(suppression));
  amp *= 1.0f - COD_BREATH_MOVE_DAMP * move_factor;
  v->breath_a += dt * COD_TAU * COD_BREATH_FREQ_A;
  v->breath_b += dt * COD_TAU * COD_BREATH_FREQ_B;
  v->breath_a -= COD_TAU * m_floorf(v->breath_a / COD_TAU);
  v->breath_b -= COD_TAU * m_floorf(v->breath_b / COD_TAU);
  f32 ba = m_sinf(v->breath_a);
  f32 bb = m_sinf(v->breath_b + 1.7f);
  f32 breath_pitch = (ba * 0.7f + bb * 0.3f) * amp;
  f32 breath_yaw = (bb * 0.75f - ba * 0.25f) * amp * 1.15f;
  f32 breath_pos = (ba * 0.6f + bb * 0.4f) * COD_BREATH_POS_AMP * (1.0f - 0.8f * move_factor);

  const CodMantle *mm = &m->mantle;
  f32 mantle_y = mm->active ? mm->cam_y : 0.0f;
  f32 mantle_fwd = mm->active ? mm->cam_forward : 0.0f;
  f32 mantle_pitch = mm->active ? mm->cam_pitch : 0.0f;
  f32 mantle_roll = mm->active ? mm->cam_roll : 0.0f;

  // death slump: the eye drops, the view tilts and rolls
  if (dead)
    v->death_time += dt;
  else
    v->death_time = 0.0f;
  f32 fall = dead ? m_minf(v->death_time / COD_SLUMP_TIME, 1.0f) : 0.0f;
  f32 slump = cod_smoothstep(fall);

  v3 base = v3_lerp(m->prev_feet, m->feet, m_saturatef(alpha));
  v->feet = base;
  base.y -= COD_CHAR_SKIN;
  f32 lateral = v->bob_offset.x + sh.translate.x;
  f32 vertical = v->bob_offset.y + v->dip.value + v->step.value + sh.translate.y + mantle_y + breath_pos - v->slide_blend * 0.1f;
  f32 forward = v->bob_offset.z + v->punch.value + mantle_fwd + v->slide_blend * 0.045f;
  v->eye_pos = (v3){base.x + m->lean_off_x + right.x * lateral + fwd.x * forward,
                    base.y + v->eye + vertical - m_fabsf(m->lean_amount) * COD_LEAN_DROP - slump * COD_SLUMP_DROP,
                    base.z + m->lean_off_z + right.z * lateral + fwd.z * forward};

  f32 pitch = m->pitch + v->recoil_pitch.value + v->kick_pitch.value + breath_pitch + v->bob_pitch + sh.pitch + mantle_pitch;
  pitch = m_clampf(pitch, -COD_PLAYER_PITCH_LIMIT, COD_PLAYER_PITCH_LIMIT) - slump * COD_SLUMP_PITCH;
  f32 yaw = m->yaw + v->recoil_yaw.value + v->kick_yaw.value + breath_yaw + sh.yaw;
  f32 roll = v->strafe_roll + v->turn_roll + v->slide_roll + v->air_roll + v->bob_roll + v->recoil_roll.value + v->kick_roll.value +
             sh.roll + mantle_roll - m->lean_amount * COD_LEAN_ROLL + slump * COD_SLUMP_ROLL;
  v->rot = quat_from_euler_yxz((v3){pitch, yaw, roll});

  // fov: critically damped, aiming crisp and sprint breathing
  f32 move_target = 1.0f;
  if (m->sliding)
    move_target = COD_FOV_SLIDE;
  else if (m->tactical)
    move_target = COD_FOV_TAC_SPRINT;
  else if (m->sprinting)
    move_target = COD_FOV_SPRINT;
  else if (!m->grounded && c->velocity.y < -6.0f)
    move_target = COD_FOV_AIR;
  v->fov_move = cod_approach(v->fov_move, move_target, COD_FOV_MOVE_TAU, dt);
  v->fov_ads = cod_approach(v->fov_ads, m_lerpf(1.0f, ads_fov_scale, ads), COD_FOV_ADS_TAU, dt);
  v->fov = base_fov * v->fov_move * v->fov_ads;

  v->move_amount = m_minf(1.0f, m->hspeed / COD_PLAYER_TAC_SPRINT_SPEED);
  v->shake_amount = camera_shake_amount(&v->shake, cod_shake_tuning());
  v->view_kick = (v3){v->recoil_pitch.value + v->kick_pitch.value, v->recoil_yaw.value + v->kick_yaw.value,
                      v->recoil_roll.value + v->kick_roll.value};
  v->view_punch = v->punch.value;
  v->placed = true;
}
