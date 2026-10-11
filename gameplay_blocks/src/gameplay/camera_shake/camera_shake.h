// Trauma-based camera shake (Eiserloh, GDC 2016): add trauma on events, shake = trauma^exponent over smooth noise.
// Apply to the camera you render with, never to state a controller reads back. Main lane, no ECS.
#pragma once

#include "lib/hzmath.h"
#include "lib/reflect.h"
#include "lib/typedefs.h"
#include "rendering/camera.h"

// zero fields take the defaults
HZ_REFLECT()
typedef struct CameraShakeTuning {
  f32 decay;       // trauma lost per second, linear; 0 -> 1.6
  f32 exponent;    // shake = trauma^exponent; 0 -> 2
  f32 frequency;   // noise lattice points per second; 0 -> 15
  f32 translation; // peak offset per metre of camera distance; 0 -> 0.018
  f32 roll;        // peak radians about the view axis; 0 -> 0.02
  f32 pitch;       // peak radians; 0 = none
  f32 yaw;         // peak radians; 0 = none
} CameraShakeTuning;

HZ_REFLECT()
typedef struct CameraShake {
  f32 trauma;
  f32 phase; // noise lattice position, wrapped at CAMERA_SHAKE_PERIOD
  u32 seed;
} CameraShake;

// camera-local: x along camera right, y along camera up, angles in radians
typedef struct CameraShakeOffset {
  v2 translate;
  f32 roll, pitch, yaw;
} CameraShakeOffset;

// the lattice hash repeats here, so wrapping the phase is seamless and f32 never loses precision
#define CAMERA_SHAKE_PERIOD 4096.0f

hz_internal CameraShakeTuning camera_shake_resolve(CameraShakeTuning t) {
  if (t.decay <= 0.0f) t.decay = 1.6f;
  if (t.exponent <= 0.0f) t.exponent = 2.0f;
  if (t.frequency <= 0.0f) t.frequency = 15.0f;
  if (t.translation <= 0.0f) t.translation = 0.018f;
  if (t.roll <= 0.0f) t.roll = 0.02f;
  return t;
}

hz_internal void camera_shake_init(CameraShake *s, u32 seed) {
  s->trauma = 0.0f;
  s->phase = 0.0f;
  s->seed = seed;
}

hz_internal void camera_shake_add(CameraShake *s, f32 amount) {
  s->trauma = m_clampf(s->trauma + amount, 0.0f, 1.0f);
}

hz_internal f32 camera_shake_amount(const CameraShake *s, CameraShakeTuning t) {
  t = camera_shake_resolve(t);
  return s->trauma > 0.0f ? m_powf(s->trauma, t.exponent) : 0.0f;
}

// gradient in [-1, 1] at an integer lattice point of one channel
hz_internal f32 camera_shake_gradient(u32 seed, u32 channel, u32 i) {
  u32 h = (i & 0xFFFu) * 0x9E3779B1u ^ (channel + 1u) * 0x85EBCA77u ^ seed * 0xC2B2AE3Du;
  h ^= h >> 15;
  h *= 0x2C1B3C6Du;
  h ^= h >> 12;
  h *= 0x297A2D39u;
  h ^= h >> 15;
  return (f32)(h & 0xFFFFFFu) / (f32)0x800000u - 1.0f;
}

// 1D Perlin noise scaled to [-1, 1]
hz_internal f32 camera_shake_noise(u32 seed, u32 channel, f32 x) {
  f32 fi = m_floorf(x);
  f32 f = x - fi;
  u32 i = (u32)fi;
  f32 a = camera_shake_gradient(seed, channel, i) * f;
  f32 b = camera_shake_gradient(seed, channel, i + 1u) * (f - 1.0f);
  f32 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
  return m_clampf(2.0f * m_lerpf(a, b, u), -1.0f, 1.0f);
}

// offsets depend only on trauma and phase, so splitting dt gives the same answer
hz_internal CameraShakeOffset camera_shake_update(CameraShake *s, CameraShakeTuning t, f32 dt,
                                                  f32 distance) {
  t = camera_shake_resolve(t);
  if (dt > 0.0f) {
    s->trauma = m_maxf(s->trauma - t.decay * dt, 0.0f);
    s->phase += t.frequency * dt;
    s->phase -= CAMERA_SHAKE_PERIOD * m_floorf(s->phase / CAMERA_SHAKE_PERIOD);
  }
  CameraShakeOffset o = {0};
  f32 k = camera_shake_amount(s, t);
  if (k <= 0.0f) return o;
  f32 move = k * t.translation * m_maxf(distance, 0.0f);
  o.translate = (v2){camera_shake_noise(s->seed, 0, s->phase) * move,
                     camera_shake_noise(s->seed, 1, s->phase) * move};
  o.roll = camera_shake_noise(s->seed, 2, s->phase) * k * t.roll;
  o.pitch = camera_shake_noise(s->seed, 3, s->phase) * k * t.pitch;
  o.yaw = camera_shake_noise(s->seed, 4, s->phase) * k * t.yaw;
  return o;
}

// moves and turns the camera in its own frame; call camera_update afterwards
hz_internal void camera_shake_apply(Camera *cam, CameraShakeOffset o) {
  cam->pos = v3_add(cam->pos, v3_add(v3_scale(camera_right(cam), o.translate.x),
                                     v3_scale(camera_up(cam), o.translate.y)));
  quat local = quat_mul(quat_rotate_y(o.yaw), quat_mul(quat_rotate_x(o.pitch), quat_rotate_z(o.roll)));
  cam->rot = quat_normalize(quat_mul(cam->rot, local));
}
