#include "camera_rig.h"
#include "lib/assert.h"

#define RIG_GROUND_PROBE 300.0f
#define RIG_BOOM_SKIN 0.05f
#define RIG_VEL_SMOOTH 12.0f
#define RIG_ORBIT_EPS 1e-6f

hz_internal f32 rig_damp(f32 current, f32 target, f32 lambda, f32 dt) {
  return lambda <= 0.0f ? target : m_damp(current, target, lambda, dt);
}

hz_internal f32 rig_or(f32 v, f32 d) { return v > 0.0f ? v : d; }

// a spring toward 0 given as stiffness k and damping c: freq = sqrt(k) / 2pi, ratio = c / (2 sqrt(k))
hz_internal void rig_spring(f32 *value, f32 *velocity, f32 stiffness, f32 damping, f32 dt) {
  f32 root_k = m_sqrtf(stiffness);
  DampedSpring s = {*value, *velocity};
  damped_spring_step(&s, 0.0f, (DampedSpringTuning){.freq = root_k / (2.0f * (f32)M_PI), .damping = damping / (2.0f * root_k)}, dt);
  *value = s.value;
  *velocity = s.velocity;
}

// the static layer when the game runs ecs_physics, else NULL
hz_internal const CollisionLayer *rig_static_layer(EcsWorld *w) {
  const EcsCollisionLayer *l = ecs_physics_get_static_layer(w);
  return l ? &l->layer : NULL;
}

// a query that hits bodies in `mask`; 0 hits everything
hz_internal PhysicsQueryFilter rig_query_filter(u32 mask) {
  return mask ? (PhysicsQueryFilter){.category = 1u, .mask = mask} : PHYSICS_QUERY_FILTER_ALL;
}

// height of the first collider under (x, z) in the categories of `mask`; false when nothing is there
hz_internal b32 rig_ground_height(const CollisionLayer *layer, f32 x, f32 z, f32 around_y, u32 mask, f32 *out) {
  if (!layer)
    return false;
  PhysicsRaycastResult r = {0};
  i32 body = -1;
  if (!physics_raycast_layer((v3){x, around_y + RIG_GROUND_PROBE, z}, (v3){x, around_y - RIG_GROUND_PROBE, z},
                             (CollisionLayer *)layer, rig_query_filter(mask), &r, &body))
    return false;
  *out = r.position.y;
  return true;
}

// ---- Follow ----

hz_internal void camera_follow_step(EcsWorld *w, const CollisionLayer *ground, const CameraRigInput *in, Camera *cam,
                                    CameraFollow *cf, CameraRigState *out, f32 dt) {
  v3 tp, tv = V3_ZERO;
  b32 grounded = true;
  quat trot = quat_identity();
  b32 have_rot = false;
  const WorldTransform *twt = cf->target ? ecs_component_get(w, cf->target, WorldTransform) : NULL;
  if (twt) {
    tp = twt->position;
    trot = twt->rotation;
    have_rot = true;
    const Character *ch = ecs_component_get(w, cf->target, Character);
    if (ch) {
      tv = v3_add(ch->velocity, ch->ground_velocity);
      grounded = character_grounded(ch);
    } else if (dt > 0.0f && cf->placed && !cf->snap) {
      v3 raw = v3_scale(v3_sub(tp, cf->prev_target), 1.0f / dt);
      tv = v3_damp(cf->target_velocity, raw, RIG_VEL_SMOOTH, dt);
    }
  } else if (in->has_target) {
    tp = in->target_position;
    tv = in->target_velocity;
    trot = in->target_rotation;
    have_rot = in->has_target_rotation;
  } else {
    return;
  }
  cf->prev_target = tp;
  cf->target_velocity = tv;

  b32 snap = !cf->placed || cf->snap;
  if (cf->distance <= 0.0f)
    cf->distance = 6.0f;
  f32 max_pitch = rig_or(cf->max_pitch, 1.4f);

  if (in->zoom != 0.0f)
    cf->distance *= m_expf(in->zoom);
  cf->distance = m_maxf(cf->distance, cf->min_distance);
  if (cf->max_distance > 0.0f)
    cf->distance = m_minf(cf->distance, cf->max_distance);

  if (cf->yaw_mode == CAMERA_FOLLOW_YAW_FREE || !have_rot) {
    cf->yaw -= in->look.x;
    cf->pitch -= in->look.y;
  } else {
    v3 f = quat_rotate(trot, V3_FORWARD);
    f32 heading = m_atan2f(f.x, f.z) + M_PI;
    f32 target_yaw = heading;
    f32 hs = m_sqrtf(tv.x * tv.x + tv.z * tv.z);
    if (cf->yaw_mode == CAMERA_FOLLOW_YAW_VELOCITY && hs > rig_or(cf->velocity_min_speed, 3.0f) &&
        v3_dot(tv, f) > 0.0f)
      target_yaw = heading + m_wrap_pi(m_atan2f(tv.x, tv.z) + M_PI - heading) * cf->velocity_align;
    cf->yaw = snap || cf->yaw_lambda <= 0.0f ? target_yaw : m_damp_angle(cf->yaw, target_yaw, cf->yaw_lambda, dt);
  }
  cf->pitch = m_clampf(cf->pitch, cf->min_pitch, max_pitch);

  v3 want = v3_add(v3_add(tp, (v3){0.0f, cf->focus_height, 0.0f}), v3_scale(tv, cf->look_ahead));
  if (snap) {
    cf->focus = want;
  } else {
    f32 up = cf->focus_lambda_up > 0.0f ? cf->focus_lambda_up : cf->focus_lambda;
    f32 air = cf->focus_lambda_air > 0.0f ? cf->focus_lambda_air : up;
    cf->focus.x = rig_damp(cf->focus.x, want.x, cf->focus_lambda, dt);
    cf->focus.z = rig_damp(cf->focus.z, want.z, cf->focus_lambda, dt);
    cf->focus.y = rig_damp(cf->focus.y, want.y, grounded ? up : air, dt);
  }

  f32 sy = m_sinf(cf->yaw), cy = m_cosf(cf->yaw), cp = m_cosf(cf->pitch);
  v3 offset = {sy * cp, m_sinf(cf->pitch), cy * cp};
  v3 pivot = v3_add(cf->focus, v3_scale((v3){cy, 0.0f, -sy}, cf->shoulder));

  f32 speed = m_sqrtf(tv.x * tv.x + tv.z * tv.z);
  f32 speed_k = cf->speed_hi > cf->speed_lo ? m_smoothstepf(cf->speed_lo, cf->speed_hi, speed) : 0.0f;
  f32 want_boom = cf->distance * (1.0f + cf->speed_pull * speed_k);
  if (ground && cf->boom_radius > 0.0f) {
    ColliderCastResult r = {0};
    i32 body = -1;
    if (physics_collider_cast_layer(collider_sphere(V3_ZERO, cf->boom_radius), pivot, quat_identity(), 1.0f,
                                    v3_add(pivot, v3_scale(offset, want_boom)), ground,
                                    rig_query_filter(cf->boom_mask), &r, &body))
      want_boom = m_maxf(cf->min_distance * 0.5f, r.distance - RIG_BOOM_SKIN);
  }
  cf->boom = snap ? want_boom : rig_damp(cf->boom, want_boom, want_boom < cf->boom ? cf->boom_in_lambda : cf->boom_out_lambda, dt);

  v3 desired = v3_add(pivot, v3_scale(offset, cf->boom));
  v3 pos = desired;
  if (cf->position_lambda > 0.0f && !snap) {
    pos.x = rig_damp(cf->position.x, desired.x, cf->position_lambda, dt);
    pos.z = rig_damp(cf->position.z, desired.z, cf->position_lambda, dt);
    pos.y = rig_damp(cf->position.y, desired.y, cf->position_lambda * 0.8f, dt);
  }
  if (cf->ground_clearance > 0.0f) {
    f32 floor = cf->floor_enabled ? cf->floor_y : -M_INFINITY;
    f32 gy;
    if (rig_ground_height(ground, pos.x, pos.z, pos.y, cf->ground_mask, &gy))
      floor = m_maxf(floor, gy);
    if (floor > -M_INFINITY && pos.y < floor + cf->ground_clearance)
      pos.y = floor + cf->ground_clearance;
  }
  cf->position = pos;

  v3 look = v3_add(pivot, v3_scale((v3){-sy, 0.0f, -cy}, cf->look_forward));
  cam->pos = pos;
  cam->rot = quat_look(v3_sub(pos, look), V3_UP);
  if (cf->near_plane > 0.0f)
    cam->near_plane = cf->near_plane;
  if (cf->far_plane > 0.0f)
    cam->far_plane = cf->far_plane;
  if (cf->fov > 0.0f) {
    if (snap)
      cam->fov = cf->fov;
    cf->fov_boost = rig_damp(cf->fov_boost, cf->fov_kick * speed_k, rig_or(cf->fov_lambda, 3.0f), dt);
    f32 fov = cf->fov + cf->fov_boost;
    if (m_absf(cam->fov - fov) > 0.01f)
      cam->fov = fov;
  }

  camera_shake_add(&cf->shake, in->shake);
  out->position = pos;
  out->shake = camera_shake_amount(&cf->shake, cf->shake_tuning);
  camera_shake_apply(cam, camera_shake_update(&cf->shake, cf->shake_tuning, dt, cf->boom));
  cf->placed = true;
  cf->snap = false;
  out->rotation = cam->rot;
  out->focus = cf->focus;
  out->yaw = cf->yaw;
  out->pitch = cf->pitch;
  out->fov = cam->fov;
  out->distance = cf->boom;
  out->zoom = 0.0f;
  out->bob_phase = 0.0f;
  out->move_amount = 0.0f;
  out->ready = true;
}

ecs_system(camera_follow_system, EcsInOut(Camera), EcsInOut(CameraFollow), EcsResIn(CameraRigInput),
           EcsResInOut(CameraRigState), EcsExternIn(WorldTransform), EcsExternIn(Character)) {
  const CameraRigInput *in = ecs_sys_get_resource(CameraRigInput);
  CameraRigState *out = ecs_sys_get_resource_mut(CameraRigState);
  for (i32 i = 0; i < it->count; i++) {
    if (ecs_sys_get_component(CameraFollow, i)->disabled)
      continue;
    camera_follow_step(it->world, NULL, in, ecs_sys_get_component_mut(Camera, i), ecs_sys_get_component_mut(CameraFollow, i),
                       out, it->delta_time);
  }
}

ecs_system(camera_follow_physics_system, EcsInOut(Camera), EcsInOut(CameraFollow), EcsResIn(CameraRigInput),
           EcsResInOut(CameraRigState), EcsResIn(EcsPhysicsWorld), EcsExternIn(WorldTransform), EcsExternIn(Character)) {
  const CameraRigInput *in = ecs_sys_get_resource(CameraRigInput);
  CameraRigState *out = ecs_sys_get_resource_mut(CameraRigState);
  const CollisionLayer *ground = rig_static_layer(it->world);
  for (i32 i = 0; i < it->count; i++) {
    if (ecs_sys_get_component(CameraFollow, i)->disabled)
      continue;
    camera_follow_step(it->world, ground, in, ecs_sys_get_component_mut(Camera, i),
                       ecs_sys_get_component_mut(CameraFollow, i), out, it->delta_time);
  }
}

// ---- Orbit ----

// ease in-out cubic
hz_internal f32 rig_ease_in_out_cubic(f32 t) {
  return t < 0.5f ? 4.0f * t * t * t : 1.0f - m_powf(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

hz_internal v3 rig_orbit_position(const CameraOrbit *co) {
  f32 r = m_sinf(co->polar) * co->distance;
  return v3_add(co->target, (v3){r * m_sinf(co->azimuth), m_cosf(co->polar) * co->distance, r * m_cosf(co->azimuth)});
}

void camera_orbit_set_view(CameraOrbit *co, v3 position, v3 target) {
  v3 offset = v3_sub(position, target);
  f32 radius = v3_length(offset);
  co->target = target;
  co->distance = radius;
  co->azimuth = m_atan2f(offset.x, offset.z);
  co->polar = radius > 0.0f ? m_acosf(m_clampf(offset.y / radius, -1.0f, 1.0f)) : 0.0f;
}

hz_internal void camera_orbit_step(const CameraRigInput *in, Camera *cam, CameraOrbit *co, CameraRigState *out, f32 dt) {
  if (co->distance <= 0.0f && !co->goto_request)
    return;
  co->placed = true;
  if (co->goto_request) {
    co->goto_request = false;
    if (co->goto_duration <= 0.0f) {
      co->flying = false;
      camera_orbit_set_view(co, co->goto_position, co->goto_target);
      co->delta_azimuth = 0.0f;
      co->delta_polar = 0.0f;
    } else {
      co->flying = true;
      co->from_position = rig_orbit_position(co);
      co->from_target = co->target;
      co->to_position = co->goto_position;
      co->to_target = co->goto_target;
      co->fly_duration = co->goto_duration;
      co->fly_t = 0.0f;
    }
  }
  if (in->cancel_fly)
    co->flying = false;
  if (co->flying) {
    co->fly_t = m_minf(1.0f, co->fly_t + dt / co->fly_duration);
    f32 k = rig_ease_in_out_cubic(co->fly_t);
    camera_orbit_set_view(co, v3_lerp(co->from_position, co->to_position, k), v3_lerp(co->from_target, co->to_target, k));
    if (co->fly_t >= 1.0f)
      co->flying = false;
  }

  co->delta_azimuth -= in->look.x;
  co->delta_polar += in->look.y;
  co->azimuth += co->auto_rotate * dt;
  if (in->zoom != 0.0f)
    co->distance *= m_expf(in->zoom);

  // OrbitControls.update; damping is per 60 Hz frame there, applied per elapsed time
  f32 damp = co->damping > 0.0f ? 1.0f - m_powf(1.0f - co->damping, dt * 60.0f) : 1.0f;
  co->azimuth += co->delta_azimuth * damp;
  co->polar += co->delta_polar * damp;
  co->delta_azimuth *= 1.0f - damp;
  co->delta_polar *= 1.0f - damp;
  if (m_fabsf(co->delta_azimuth) < RIG_ORBIT_EPS)
    co->delta_azimuth = 0.0f;
  if (m_fabsf(co->delta_polar) < RIG_ORBIT_EPS)
    co->delta_polar = 0.0f;
  co->polar = m_clampf(co->polar, co->min_polar, co->max_polar > 0.0f ? co->max_polar : M_PI);
  co->polar = m_clampf(co->polar, RIG_ORBIT_EPS, M_PI - RIG_ORBIT_EPS);
  co->distance = m_maxf(co->distance, co->min_distance);
  if (co->max_distance > 0.0f)
    co->distance = m_minf(co->distance, co->max_distance);

  v3 pos = rig_orbit_position(co);
  cam->pos = pos;
  camera_look_at(cam, co->target);
  if (co->near_plane > 0.0f)
    cam->near_plane = co->near_plane;
  if (co->far_plane > 0.0f)
    cam->far_plane = co->far_plane;
  if (co->fov > 0.0f)
    cam->fov = co->fov;

  camera_shake_add(&co->shake, in->shake);
  out->shake = camera_shake_amount(&co->shake, co->shake_tuning);
  camera_shake_apply(cam, camera_shake_update(&co->shake, co->shake_tuning, dt, co->distance));
  out->position = pos;
  out->rotation = cam->rot;
  out->focus = co->target;
  out->yaw = co->azimuth + M_PI;
  out->pitch = M_PI * 0.5f - co->polar;
  out->fov = cam->fov;
  out->distance = co->distance;
  out->zoom = 0.0f;
  out->bob_phase = 0.0f;
  out->move_amount = 0.0f;
  out->ready = true;
}

ecs_system(camera_orbit_system, EcsInOut(Camera), EcsInOut(CameraOrbit), EcsResIn(CameraRigInput),
           EcsResInOut(CameraRigState)) {
  const CameraRigInput *in = ecs_sys_get_resource(CameraRigInput);
  CameraRigState *out = ecs_sys_get_resource_mut(CameraRigState);
  for (i32 i = 0; i < it->count; i++) {
    if (ecs_sys_get_component(CameraOrbit, i)->disabled)
      continue;
    camera_orbit_step(in, ecs_sys_get_component_mut(Camera, i), ecs_sys_get_component_mut(CameraOrbit, i), out,
                      it->delta_time);
  }
}

// ---- First person ----

hz_internal void camera_fp_step(EcsWorld *w, const CameraRigInput *in, Camera *cam, CameraFirstPerson *fp,
                                CameraRigState *out, f32 dt) {
  const WorldTransform *twt = fp->target ? ecs_component_get(w, fp->target, WorldTransform) : NULL;
  if (!twt)
    return;
  const Character *ch = ecs_component_get(w, fp->target, Character);
  const CharacterInput *chin = ecs_component_get(w, fp->target, CharacterInput);
  const CharacterTuning *tun = ecs_component_get(w, fp->target, CharacterTuning);
  CharacterTuning t = character_tuning_resolve(tun ? *tun : (CharacterTuning){0});

  if (!fp->placed || fp->snap) {
    fp->zoom = 0.0f;
    fp->bob_phase = 0.0f;
    fp->move_amount = 0.0f;
    fp->roll = 0.0f;
    fp->fov_kick = 0.0f;
    fp->recoil_pitch = fp->recoil_yaw = 0.0f;
    fp->dip = fp->dip_velocity = 0.0f;
    fp->punch = fp->punch_velocity = 0.0f;
    fp->death_time = 0.0f;
    fp->shake.trauma = 0.0f;
    fp->landings_seen = ch ? ch->landings : 0;
    fp->placed = true;
    fp->snap = false;
  }

  f32 limit = rig_or(fp->pitch_limit, 1.52f);
  fp->yaw -= in->look.x;
  fp->pitch = m_clampf(fp->pitch + in->look.y, -limit, limit);
  fp->recoil_yaw -= in->recoil.x;
  fp->recoil_pitch += in->recoil.y;
  fp->punch_velocity += in->punch;
  fp->fov_kick += in->fov_kick;
  camera_shake_add(&fp->shake, in->shake);

  b32 grounded = ch ? character_grounded(ch) : true;
  f32 speed = ch ? character_planar_speed(ch) : 0.0f;
  if (ch && ch->landings != fp->landings_seen) {
    if (ch->land_speed > rig_or(fp->dip_min_speed, 3.0f))
      fp->dip_velocity += -m_minf(ch->land_speed, rig_or(fp->dip_max_speed, 18.0f)) * rig_or(fp->dip_scale, 0.12f);
    fp->landings_seen = ch->landings;
  }

  fp->recoil_pitch = rig_damp(fp->recoil_pitch, 0.0f, rig_or(fp->recoil_lambda, 7.0f), dt);
  fp->recoil_yaw = rig_damp(fp->recoil_yaw, 0.0f, rig_or(fp->recoil_lambda, 7.0f), dt);
  rig_spring(&fp->dip, &fp->dip_velocity, rig_or(fp->dip_stiffness, 160.0f), rig_or(fp->dip_damping, 14.0f), dt);
  rig_spring(&fp->punch, &fp->punch_velocity, rig_or(fp->punch_stiffness, 220.0f), rig_or(fp->punch_damping, 18.0f), dt);
  fp->zoom = rig_damp(fp->zoom, fp->zoom_target, rig_or(fp->zoom_lambda, 14.0f), dt);

  f32 ref = rig_or(fp->bob_speed_ref, t.run_speed);
  fp->move_amount = rig_damp(fp->move_amount, grounded ? m_minf(speed / ref, 1.0f) : 0.0f, 10.0f, dt);
  if (grounded)
    fp->bob_phase += speed * dt * rig_or(fp->bob_rate, 1.35f);
  f32 strafe = chin ? chin->move.x : 0.0f;
  fp->roll = rig_damp(fp->roll, -strafe * rig_or(fp->roll_strafe, 0.012f) * (grounded ? 1.0f : rig_or(fp->roll_air_scale, 0.4f)),
                      rig_or(fp->roll_lambda, 8.0f), dt);
  b32 sprint = ch && ch->running && fp->zoom_target <= 0.0f;
  fp->fov_kick = rig_damp(fp->fov_kick, sprint ? rig_or(fp->run_fov_kick, 6.0f) : 0.0f, rig_or(fp->fov_kick_lambda, 5.0f), dt);

  if (in->dead)
    fp->death_time += dt;
  else
    fp->death_time = 0.0f;
  f32 fall = in->dead ? m_minf(fp->death_time / rig_or(fp->slump_time, 0.9f), 1.0f) : 0.0f;
  f32 ease = fall * fall * (3.0f - 2.0f * fall);

  CameraShakeOffset shake = camera_shake_update(&fp->shake, fp->shake_tuning, dt, 0.0f);
  f32 bob = fp->move_amount * (1.0f - fp->zoom * rig_or(fp->bob_zoom_damp, 0.8f));
  f32 bob_y = m_absf(m_sinf(fp->bob_phase)) * rig_or(fp->bob_height, 0.045f) * bob;
  f32 bob_x = m_cosf(fp->bob_phase) * rig_or(fp->bob_sway, 0.025f) * bob;

  v3 eye = v3_add(twt->position, (v3){0.0f, character_eye_height(&t, ch), 0.0f});
  eye.y += bob_y + fp->dip - ease * rig_or(fp->slump_drop, 1.25f);
  eye.x += m_cosf(fp->yaw) * bob_x;
  eye.z -= m_sinf(fp->yaw) * bob_x;
  cam->pos = eye;
  cam->rot = quat_from_euler_yxz((v3){
      fp->pitch + fp->recoil_pitch + shake.pitch + fp->punch * rig_or(fp->punch_pitch, 0.02f) -
          ease * rig_or(fp->slump_pitch, 0.35f),
      fp->yaw + fp->recoil_yaw + shake.yaw,
      fp->roll + shake.roll + ease * rig_or(fp->slump_roll, 0.6f),
  });

  if (fp->near_plane > 0.0f)
    cam->near_plane = fp->near_plane;
  if (fp->far_plane > 0.0f)
    cam->far_plane = fp->far_plane;
  if (fp->fov > 0.0f) {
    f32 zoom_fov = rig_or(fp->zoom_fov, fp->fov);
    f32 fov = m_lerpf(fp->fov, zoom_fov, fp->zoom) + fp->fov_kick * (1.0f - fp->zoom);
    if (m_absf(cam->fov - fov) > 0.01f)
      cam->fov = fov;
  }

  out->position = eye;
  out->rotation = cam->rot;
  out->focus = twt->position;
  out->yaw = fp->yaw;
  out->pitch = fp->pitch;
  out->fov = cam->fov;
  out->distance = 0.0f;
  out->zoom = fp->zoom;
  out->bob_phase = fp->bob_phase;
  out->move_amount = fp->move_amount;
  out->shake = camera_shake_amount(&fp->shake, fp->shake_tuning);
  out->ready = true;
}

ecs_system(camera_first_person_system, EcsInOut(Camera), EcsInOut(CameraFirstPerson), EcsResIn(CameraRigInput),
           EcsResInOut(CameraRigState), EcsExternIn(WorldTransform), EcsExternIn(Character), EcsExternIn(CharacterInput),
           EcsExternIn(CharacterTuning)) {
  const CameraRigInput *in = ecs_sys_get_resource(CameraRigInput);
  CameraRigState *out = ecs_sys_get_resource_mut(CameraRigState);
  for (i32 i = 0; i < it->count; i++) {
    if (ecs_sys_get_component(CameraFirstPerson, i)->disabled)
      continue;
    camera_fp_step(it->world, in, ecs_sys_get_component_mut(Camera, i), ecs_sys_get_component_mut(CameraFirstPerson, i), out,
                   it->delta_time);
  }
}

void camera_rig_init(EcsWorld *w) {
  assert_msg(is_main_thread(), "camera_rig_init: main lane only"); // ECS resources are set outside a scope
  ecs_resource_set(w, CameraRigInput, ((CameraRigInput){0}));
  ecs_resource_set(w, CameraRigState, ((CameraRigState){.rotation = quat_identity()}));
}

void camera_rig_schedule(EcsWorld *w) {
  if (ecs_resource_get(w, EcsPhysicsWorld))
    ecs_schedule_single(w, camera_follow_physics_system);
  else
    ecs_schedule_single(w, camera_follow_system);
  ecs_schedule_single(w, camera_orbit_system);
  ecs_schedule_single(w, camera_first_person_system);
}
