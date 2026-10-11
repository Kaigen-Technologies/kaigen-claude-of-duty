// Claude of Duty player internals: the tuning, its spring and easing maths and the runtime components.
#pragma once
#include "ecs/ecs.h"
#include "ecs/ecs_physics/ecs_physics.h"
#include "gameplay/camera_rig/camera_rig.h"
#include "gameplay/character/character.h"
#include "lib/hzmath.h"
#include "lib/typedefs.h"
#include "physics/physics.h"
#include "claude_of_duty.h"
#include "cod_player.h"

#define COD_DEG 0.017453292519943295f
#define COD_PI 3.14159265358979f
#define COD_TAU 6.283185307179586f

// ---- stance tuning ----
#define COD_PLAYER_WALK_SPEED 4.57f
#define COD_PLAYER_SPRINT_SPEED 7.01f
#define COD_PLAYER_TAC_SPRINT_SPEED 8.38f
#define COD_PLAYER_JUMP_APEX 0.6f
#define COD_PLAYER_PRONE_HEIGHT 0.7f
#define COD_PLAYER_PITCH_LIMIT (88.0f * COD_DEG)
#define COD_PLAYER_MOUSE_SENSITIVITY 0.0022f // .sensitivity, radians per pixel
#define COD_PLAYER_ADS_SENS_SCALE 0.65f      // .adsSensScale
#define COD_PLAYER_ADS_FOV_SCALE 0.72f       // .adsFovScale
#define COD_PLAYER_FALL_RESET_Y -20.0f
#define COD_PLAYER_RESPAWN_DELAY 3.0f
// the gap the character block keeps to every surface: the capsule touches the floor
#define COD_CHAR_SKIN 0.02f

// ---- movement ----
#define COD_MOVE_STRAFE_SCALE 0.92f
#define COD_MOVE_BACK_SCALE 0.8f
#define COD_MOVE_ADS_SCALE 0.5f
#define COD_MOVE_GROUND_ACCEL 92.0f
#define COD_MOVE_GROUND_DECEL 52.0f
#define COD_MOVE_STOP_DECEL 30.0f
#define COD_MOVE_AIR_ACCEL_SCALE 0.25f
#define COD_MOVE_AIR_SPEED_CAP 3.4f
#define COD_MOVE_COYOTE_TIME 0.09f
#define COD_MOVE_JUMP_BUFFER 0.13f
#define COD_MOVE_JUMP_COOLDOWN 0.28f
#define COD_MOVE_TAC_TAP_WINDOW 0.32f
#define COD_MOVE_TAC_MAX_TIME 6.0f
#define COD_MOVE_TAC_RECOVERY 1.6f
#define COD_MOVE_SPRINT_FORWARD_DOT 0.55f
#define COD_MOVE_SPRINT_START_DELAY 0.05f

#define COD_SLIDE_ENTRY_SPEED 8.84f
#define COD_SLIDE_MIN_ENTRY 6.2f
#define COD_SLIDE_EXIT_SPEED 2.95f
#define COD_SLIDE_DURATION 0.9f
#define COD_SLIDE_DRAG 0.75f
#define COD_SLIDE_BRAKE 0.85f
#define COD_SLIDE_COOLDOWN 0.55f
#define COD_SLIDE_MIN_SPEED_TO_START 5.2f
#define COD_SLIDE_STEER 2.6f
#define COD_SLIDE_SLOPE_ASSIST 9.0f

#define COD_MANTLE_AUTO_VAULT_MAX 0.72f
#define COD_MANTLE_MIN_HEIGHT 0.34f
#define COD_MANTLE_MAX_HEIGHT 1.85f
#define COD_MANTLE_REACH 0.62f
#define COD_MANTLE_LAND_DEPTH 0.46f
#define COD_MANTLE_VAULT_TIME 0.34f
#define COD_MANTLE_MANTLE_TIME 0.62f
#define COD_MANTLE_HIGH_TIME 0.82f
#define COD_MANTLE_COOLDOWN 0.2f
#define COD_MANTLE_AUTO_SPEED 2.4f
#define COD_MANTLE_PROACTIVE_DISTANCE 0.2f
#define COD_MANTLE_PROACTIVE_LOOKAHEAD 0.035f

#define COD_LEAN_OFFSET 0.34f
#define COD_LEAN_ROLL (13.0f * COD_DEG)
#define COD_LEAN_DROP 0.035f
#define COD_LEAN_RATE 0.085f
#define COD_LEAN_PROBE_RADIUS 0.17f

#define COD_STANCE_TAU_STAND_CROUCH 0.062f
#define COD_STANCE_TAU_CROUCH_STAND 0.072f
#define COD_STANCE_TAU_PRONE 0.16f

// ---- camera ----
#define COD_BOB_AMP_X 0.0165f
#define COD_BOB_AMP_Y 0.0115f
#define COD_BOB_AMP_Z 0.006f
#define COD_BOB_ROLL (0.42f * COD_DEG)
#define COD_BOB_PITCH (0.16f * COD_DEG)
#define COD_BOB_SPEED_EXP 0.85f
#define COD_BOB_SPEED_CAP 1.55f
#define COD_BOB_ADS_SCALE 0.22f
#define COD_BOB_AIR_FADE 0.11f

#define COD_STEP_IMPULSE 0.085f
#define COD_STEP_FREQ 5.4f
#define COD_STEP_DAMPING 0.62f
#define COD_STEP_SPRINT_SCALE 1.7f

#define COD_LAND_MIN_SPEED 2.2f
#define COD_LAND_FULL_SPEED 12.5f
#define COD_LAND_DIP_IMPULSE 2.35f
#define COD_LAND_PITCH (3.4f * COD_DEG)
#define COD_LAND_ROLL (0.9f * COD_DEG)
#define COD_LAND_FREQ 3.05f
#define COD_LAND_DAMPING 0.52f
#define COD_LAND_TRAUMA 0.34f
#define COD_LAND_DAMAGE_SPEED 15.0f
#define COD_LAND_DAMAGE_PER_SPEED 7.0f

#define COD_ROLL_STRAFE (1.05f * COD_DEG)
#define COD_ROLL_YAW_RATE 0.055f
#define COD_ROLL_YAW_RATE_MAX (1.5f * COD_DEG)
#define COD_ROLL_TAU 0.11f
#define COD_ROLL_SLIDE (5.2f * COD_DEG)
#define COD_ROLL_AIR (0.9f * COD_DEG)

#define COD_RECOIL_FREQ 9.5f
#define COD_RECOIL_DAMPING 0.5f
#define COD_RECOIL_RESIDUAL_TAU 0.28f
#define COD_RECOIL_RESIDUAL_SHARE 0.34f
#define COD_RECOIL_PUNCH_FREQ 12.0f
#define COD_RECOIL_PUNCH_DAMPING 0.62f

#define COD_SHAKE_DECAY 1.85f
#define COD_SHAKE_ROT (1.35f * COD_DEG)
#define COD_SHAKE_POS 0.022f
#define COD_SHAKE_FREQ 22.0f

#define COD_BREATH_FREQ_A 0.235f
#define COD_BREATH_FREQ_B 0.155f
#define COD_BREATH_AMP 0.0021f
#define COD_BREATH_POS_AMP 0.0035f
#define COD_BREATH_ADS_SCALE 1.85f
#define COD_BREATH_LOW_HEALTH_SCALE 2.6f
#define COD_BREATH_MOVE_DAMP 0.78f
#define COD_BREATH_SUPPRESSION_SCALE 2.2f

#define COD_FOV_SPRINT 1.055f
#define COD_FOV_TAC_SPRINT 1.1f
#define COD_FOV_SLIDE 1.085f
#define COD_FOV_AIR 1.015f
#define COD_FOV_ADS_TAU 0.052f
#define COD_FOV_MOVE_TAU 0.13f

// the death slump: the eye drops, the view tilts and rolls (camera_rig CameraFirstPerson defaults)
#define COD_SLUMP_TIME 0.9f
#define COD_SLUMP_DROP 1.25f
#define COD_SLUMP_PITCH 0.35f
#define COD_SLUMP_ROLL 0.6f

// ---- health ----
#define COD_HEALTH_REGEN_DELAY 4.6f
#define COD_HEALTH_REGEN_RATE 34.0f
#define COD_HEALTH_REGEN_RAMP 0.55f
#define COD_HEALTH_LOW 0.36f
#define COD_HEALTH_CRITICAL 0.18f
#define COD_HEALTH_INDICATOR_TIME 1.8f
#define COD_SUPPRESSION_PER_HIT 0.5f
#define COD_SUPPRESSION_PER_EXPLOSION 0.85f
#define COD_SUPPRESSION_DECAY 0.62f
#define COD_SUPPRESSION_SHAKE_SCALE 0.28f
#define COD_HIT_FLASH 0.85f
#define COD_HIT_FLASH_TAU 0.22f
#define COD_HEARTBEAT_MIN 1.05f
#define COD_HEARTBEAT_MAX 2.05f

// ---- footsteps ----
#define COD_FOOT_LATERAL 0.13f
#define COD_FOOT_PROBE 0.9f
#define COD_FOOT_RUN_SPEED 5.4f
#define COD_FOOT_LAND_HOLD 0.12f

// ---- springs ----
hz_internal f32 cod_smootherstep(f32 t) {
  t = m_saturatef(t);
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

hz_internal f32 cod_smoothstep(f32 t) { return m_smoothf(m_saturatef(t)); }

// exponential approach with a real time constant: `tau` is the 63 % time
hz_internal f32 cod_approach(f32 current, f32 target, f32 tau, f32 dt) {
  if (tau <= 1e-6f)
    return target;
  return target + (current - target) * m_expf(-dt / tau);
}

// a damped harmonic oscillator by frequency (Hz) and damping ratio; impulses add velocity
typedef struct CodSpring {
  f32 freq;
  f32 damping;
  f32 value;
  f32 velocity;
  f32 target;
} CodSpring;

#define COD_SPRING_MAX_SUB_DT (1.0f / 360.0f)

// sub-stepped so a stiff spring stays stable through a dropped frame; it stops after 24 sub-steps and drops the rest of a long frame
hz_internal void cod_spring_step(CodSpring *s, f32 dt) {
  if (dt <= 0.0f)
    return;
  f32 w = COD_TAU * s->freq;
  f32 k = w * w;
  f32 c = 2.0f * s->damping * w;
  i32 n = (i32)m_ceilf(dt / COD_SPRING_MAX_SUB_DT);
  f32 h = dt / (f32)n;
  for (i32 i = 0; i < n; i++) {
    f32 a = -k * (s->value - s->target) - c * s->velocity;
    s->velocity += a * h;
    s->value += s->velocity * h;
  }
  if (m_fabsf(s->value - s->target) < 1e-7f && m_fabsf(s->velocity) < 1e-6f) {
    s->value = s->target;
    s->velocity = 0.0f;
  }
}

// a fast under-damped spring plus a slow exponential residual: recoil rises at once, snaps most of the way back, then settles
typedef struct CodRecoil {
  CodSpring spring;
  f32 residual;
  f32 residual_tau;
  f32 residual_share;
  f32 value;
} CodRecoil;

hz_internal CodRecoil cod_recoil_make(f32 freq, f32 damping, f32 tau, f32 share) {
  return (CodRecoil){.spring = {.freq = freq, .damping = damping}, .residual_tau = tau, .residual_share = share};
}

hz_internal void cod_recoil_kick(CodRecoil *r, f32 amount) {
  r->spring.value += amount * (1.0f - r->residual_share);
  r->residual += amount * r->residual_share;
}

hz_internal void cod_recoil_step(CodRecoil *r, f32 dt) {
  cod_spring_step(&r->spring, dt);
  r->residual = cod_approach(r->residual, 0.0f, r->residual_tau, dt);
  r->value = r->spring.value + r->residual;
}

// ---- stances ----
typedef struct CodStanceDef {
  f32 height;
  f32 eye;
  f32 speed;
  f32 step_height;
  f32 stride;
} CodStanceDef;

hz_internal CodStanceDef cod_stance_def(u32 stance) {
  switch (stance) {
  case COD_STANCE_CROUCH: return (CodStanceDef){COD_PLAYER_CROUCH_HEIGHT, COD_PLAYER_CROUCH_HEIGHT - 0.1f, 2.44f, 0.3f, 1.05f};
  case COD_STANCE_PRONE: return (CodStanceDef){COD_PLAYER_PRONE_HEIGHT, 0.4f, 1.01f, 0.14f, 0.78f};
  default: return (CodStanceDef){COD_PLAYER_HEIGHT, COD_PLAYER_HEIGHT - COD_EYE_OFFSET, COD_PLAYER_WALK_SPEED, 0.42f, 1.48f};
  }
}

// friction, indexed by CodSurface
hz_internal f32 cod_surface_friction(u32 surface) {
  switch (surface) {
  case COD_SURFACE_CONCRETE: return 0.92f;
  case COD_SURFACE_METAL: return 0.52f;
  case COD_SURFACE_WOOD: return 0.72f;
  case COD_SURFACE_DIRT: return 0.96f;
  case COD_SURFACE_SAND: return 1.05f;
  case COD_SURFACE_GLASS: return 0.32f;
  case COD_SURFACE_WATER: return 0.3f;
  case COD_SURFACE_FOLIAGE: return 0.62f;
  case COD_SURFACE_FABRIC: return 0.8f;
  case COD_SURFACE_FLESH: return 0.9f;
  case COD_SURFACE_RUBBER: return 1.25f;
  case COD_SURFACE_PLASTER: return 0.86f;
  default: return 0.9f;
  }
}

// ---- the physics the movement queries: the character's own layers ----
typedef struct CodPhys {
  EcsWorld *w;
  const EcsCollisionLayer *layers[2]; // static, kinematic; NULL until built
} CodPhys;

typedef struct CodHit {
  b32 hit;
  v3 point;
  v3 normal;
  f32 distance;
  u32 surface; // CodSurface
} CodHit;

// ---- the ledge probe and the rooted climb ----
typedef struct CodLedge {
  u32 kind; // CodLedgeKind
  b32 fast;
  f32 obstacle_height;
  f32 top_y;
  f32 lip_x;
  f32 lip_z;
  v3 land;
  f32 distance;
  u32 surface;
} CodLedge;

typedef struct CodMantle {
  b32 active;
  u32 kind; // CodLedgeKind
  f32 t;
  f32 duration;
  v3 start;
  f32 top_y;
  v3 land;
  f32 fx;
  f32 fz;
  f32 side;
  f32 height;
  f32 exit_speed;
  v3 pos; // floor-level capsule bottom
  f32 cam_y;
  f32 cam_forward;
  f32 cam_pitch;
  f32 cam_roll;
} CodMantle;

// ---- components ----

// the frame's held intent, latched on the main input system
typedef struct CodCmd {
  v2 move;
  b32 jump_held;
  b32 sprint_held;
  b32 lean_left;
  b32 lean_right;
} CodCmd;

// the movement state machine and everything the fixed step owns; runtime only
typedef struct CodMove {
  b32 placed;
  u32 applied_restart;
  u32 applied_pose;

  f32 yaw;      // the movement basis; forward = (-sin, 0, -cos)
  f32 pitch;
  f32 yaw_rate;

  CodCmd cmd;
  b32 edge_jump;   // presses latched until a fixed step takes them
  b32 edge_crouch;
  b32 edge_prone;
  b32 edge_sprint;
  b32 controls;

  u32 state;       // CodMoveState
  u32 prev_state;
  f32 state_time;
  u32 stance;      // CodStance, the physical capsule
  u32 stance_want;
  b32 sprinting;
  b32 tactical;
  b32 sliding;
  b32 grounded;
  b32 was_grounded;
  b32 blocked;
  b32 suppress_landing;
  b32 rooted;      // a mantle step drove the capsule: no post-move bookkeeping
  u32 state_sig;   // the discrete state last published, to flag changes
  f32 air_time;
  f32 ground_time;
  f32 speed;
  f32 hspeed;
  f32 ads;
  b32 ads_requested;
  f32 move_speed_scale;

  f32 lean_amount;
  f32 lean_allowed;
  f32 lean_off_x;
  f32 lean_off_z;
  f32 lean_probe_timer;

  f32 coyote;
  f32 jump_buffer;
  f32 jump_cooldown;
  f32 sprint_hold;
  f32 last_sprint_press;
  f32 tac_time;
  f32 tac_lock;
  b32 tac_requested;
  f32 slide_time;
  f32 slide_cooldown;
  f32 slide_dir_x;
  f32 slide_dir_z;
  f32 slide_side;
  f32 mantle_cooldown;
  f32 ledge_probe_timer;
  f32 step_distance;
  f32 bob_phase; // gait phase, pi per footfall
  f32 foot_hold;
  b32 foot_left;
  f32 prev_vy;
  v2 intent_vel;  // the horizontal velocity the last step asked for

  v3 prev_feet;   // the capsule bottom at the start of the last step and at its end, for interpolation
  v3 feet;
  CodMantle mantle;
  CodLedge ledge;

  u32 jumps_seen;
  u32 landings_seen;
  u32 teleports_seen;
  u32 teleports_requested;
  u32 teleports_acked;

  CodPlayerEvents ev;  // this frame's movement events, cleared by the input system
  u32 steps_pending;   // footsteps this frame, for the camera's per-footfall spring
  b32 step_running;
  u32 step_stance;
} CodMove;
ECS_COMPONENT_TYPEDEF(CodMove);

// health, regeneration, suppression, damage direction, the low-health treatment weights; runtime only
typedef struct CodHealth {
  f32 value;
  b32 dead;
  f32 dead_time;
  b32 regenerating;
  f32 since_damage;
  f32 suppression;
  f32 hit_flash;
  f32 effect;
  f32 beat_phase;
  f32 pulse;
  CodDamageIndicator indicators[COD_PLAYER_MAX_INDICATORS];
  u32 hit_count;
  CodPlayerHit hits[COD_PLAYER_MAX_HITS];
  b32 died;
  b32 respawned;
  b32 beat;
  f32 beat_strength;
} CodHealth;
ECS_COMPONENT_TYPEDEF(CodHealth);

// the camera feel: smoothed eye, bob, springs, rolls, breathing, shake, fov; runtime only
typedef struct CodView {
  EcsEntity camera;
  b32 placed;
  f32 eye;
  f32 bob_phase;
  f32 bob_weight;
  f32 bob_roll;
  f32 bob_pitch;
  v3 bob_offset;
  CodSpring dip;
  CodSpring step;
  CodSpring punch;
  CodRecoil recoil_pitch;
  CodRecoil recoil_yaw;
  CodRecoil recoil_roll;
  CodRecoil kick_pitch;
  CodRecoil kick_yaw;
  CodRecoil kick_roll;
  f32 strafe_roll;
  f32 turn_roll;
  f32 slide_roll;
  f32 air_roll;
  f32 slide_blend;
  f32 slide_side;
  CameraShake shake;
  f32 breath_a;
  f32 breath_b;
  f32 fov_move;
  f32 fov_ads;
  f32 death_time;
  v3 feet;         // outputs: the interpolated capsule bottom
  v3 eye_pos;
  quat rot;
  f32 fov;
  f32 move_amount;
  f32 shake_amount;
  v3 view_kick;
  f32 view_punch;
} CodView;
ECS_COMPONENT_TYPEDEF(CodView);

// on the camera entity: the player whose view it shows
typedef struct CodCameraLink {
  EcsEntity player;
} CodCameraLink;
ECS_COMPONENT_TYPEDEF(CodCameraLink);

// the dev test course: spawned once while CodDevSwitches.player_course is set
typedef struct CodCourse {
  b32 spawned;
  b32 trace; // CodDevSwitches.player_trace
  EcsEntity boxes[16];
  u32 count;
} CodCourse;
ECS_COMPONENT_TYPEDEF(CodCourse);
