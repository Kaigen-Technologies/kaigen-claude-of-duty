// Camera rigs: CameraFollow (third person), CameraOrbit (OrbitControls) and CameraFirstPerson, driving the entity's Camera component.

// Follow: damped focus, look-ahead, boom with collision, ground clearance, speed pull and FOV kick.

// Orbit: damping, polar and distance limits, fly-to / jump-to with easeInOutCubic.

// First person: head bob, landing dip and punch springs, recoil, ADS zoom and the death slump.

// All three apply camera_shake trauma. The game writes CameraRigInput each frame, whole (look and zoom are per-frame deltas).

// Rigs write Camera directly: run ecs_camera_schedule with transform_sync_off. Pointer lock stays with input.

// look.x is radians turned right, look.y radians tilted up; zoom is a log factor: distance *= exp(zoom).

// Rigs run on one lane (ecs_schedule_single) and publish CameraRigState for camera-relative input.

// Boom and ground queries use the static layer of EcsPhysicsWorld when it exists; masks pick the colliders that count.
#pragma once

#include "ecs/camera/ecs_camera.h"
#include "ecs/ecs.h"
#include "gameplay/camera_shake/camera_shake.h"
#include "gameplay/character/character.h"
#include "lib/hzmath.h"
#include "lib/reflect.h"
#include "rendering/camera.h"

HZ_REFLECT()
typedef enum {
  CAMERA_FOLLOW_YAW_FREE = 0,   // yaw from CameraRigInput.look
  CAMERA_FOLLOW_YAW_TARGET,     // behind the target's facing
  CAMERA_FOLLOW_YAW_VELOCITY,   // behind the target's facing, blended toward its travel direction
} CameraFollowYaw;

// zero numeric fields mean off or instant unless noted; `disabled` hands the Camera to game code
HZ_REFLECT()
ECS_COMPONENT(CameraFollow) {
  b32 disabled;
  HZ_RUNTIME b32 snap;               // request, consumed: place focus, boom and yaw at their targets at once
  EcsEntity target;         // followed entity (WorldTransform; Character when present); 0 -> CameraRigInput.target_*
  f32 focus_height;         // up from the target origin to the pivot
  f32 shoulder;             // pivot offset along the camera's right
  f32 look_ahead;           // seconds of target velocity added to the pivot
  f32 look_forward;         // metres the look point sits ahead of the pivot along the view yaw
  f32 yaw;                  // radians, forward = (-sin, 0, -cos)
  f32 pitch;                // camera elevation above the pivot, radians
  f32 min_pitch;
  f32 max_pitch;            // 0 -> 1.4
  CameraFollowYaw yaw_mode;
  f32 yaw_lambda;           // 1/s toward the mode's yaw; 0 -> snap
  f32 velocity_align;       // YAW_VELOCITY blend toward the travel direction, 0..1
  f32 velocity_min_speed;   // m/s below which travel direction is ignored; 0 -> 3
  f32 distance;             // boom length the zoom aims for; the zoom scales it
  f32 min_distance;
  f32 max_distance;         // 0 -> unlimited
  f32 focus_lambda;         // horizontal pivot damping, 1/s; 0 -> rigid
  f32 focus_lambda_up;      // vertical, grounded target; 0 -> focus_lambda
  f32 focus_lambda_air;     // vertical, airborne target; 0 -> focus_lambda_up
  f32 position_lambda;      // the camera's own lag behind its desired spot; 0 -> none
  f32 boom_in_lambda;       // boom shortening rate (obstacle, zoom in); 0 -> instant
  f32 boom_out_lambda;      // boom recovery rate; 0 -> instant
  f32 speed_lo;             // speed range for the pull and FOV kick
  f32 speed_hi;
  f32 speed_pull;           // extra boom fraction at speed_hi
  f32 boom_radius;          // sphere cast radius against the static layer; 0 -> no boom collision
  u32 boom_mask;            // collision categories the boom hits; 0 -> all
  f32 ground_clearance;     // metres kept above the ground below the camera; 0 -> off
  u32 ground_mask;          // categories that count as ground; 0 -> all
  b32 floor_enabled;
  f32 floor_y;              // the camera never goes below floor_y + ground_clearance (the sea)
  f32 fov;                  // degrees; 0 -> leave the Camera's
  f32 fov_kick;             // degrees added at speed_hi
  f32 fov_lambda;           // 0 -> 3
  f32 near_plane;           // 0 -> leave the Camera's
  f32 far_plane;
  CameraShakeTuning shake_tuning;
  // runtime
  HZ_RUNTIME v3 focus;
  HZ_RUNTIME v3 position;
  HZ_RUNTIME v3 prev_target;
  HZ_RUNTIME v3 target_velocity;
  HZ_RUNTIME f32 boom;
  HZ_RUNTIME f32 fov_boost;
  HZ_RUNTIME CameraShake shake;
  HZ_RUNTIME b32 placed;
};

HZ_REFLECT()
ECS_COMPONENT(CameraOrbit) {
  b32 disabled;
  // the view: placed by camera_orbit_set_view or a goto request, never authored
  HZ_RUNTIME v3 target;
  HZ_RUNTIME f32 distance;
  HZ_RUNTIME f32 azimuth;    // radians around Y from +Z
  HZ_RUNTIME f32 polar;      // radians from +Y
  f32 damping;               // OrbitControls dampingFactor per 60 Hz frame, 0..1; 0 -> none
  f32 min_polar;
  f32 max_polar;             // 0 -> pi
  f32 min_distance;
  f32 max_distance;          // 0 -> unlimited
  f32 auto_rotate;           // rad/s around the target
  f32 fov;                   // 0 -> leave the Camera's
  f32 near_plane;
  f32 far_plane;
  HZ_RUNTIME b32 goto_request; // consumed: fly (or jump when duration <= 0) to the goto_* view
  HZ_RUNTIME v3 goto_position;
  HZ_RUNTIME v3 goto_target;
  HZ_RUNTIME f32 goto_duration;
  CameraShakeTuning shake_tuning;
  // runtime
  HZ_RUNTIME f32 delta_azimuth;
  HZ_RUNTIME f32 delta_polar;
  HZ_RUNTIME b32 flying;
  HZ_RUNTIME v3 from_position;
  HZ_RUNTIME v3 from_target;
  HZ_RUNTIME v3 to_position;
  HZ_RUNTIME v3 to_target;
  HZ_RUNTIME f32 fly_duration;
  HZ_RUNTIME f32 fly_t;
  HZ_RUNTIME CameraShake shake;
  HZ_RUNTIME b32 placed;
};

HZ_REFLECT()
ECS_COMPONENT(CameraFirstPerson) {
  b32 disabled;
  HZ_RUNTIME b32 snap;                // request, consumed: clear springs, zoom, trauma, recoil and the slump
  EcsEntity target;          // the Character: eye = its WorldTransform + CharacterTuning.eye_height
  f32 yaw;                   // look yaw, radians, + turns left
  f32 pitch;                 // + looks up
  f32 pitch_limit;           // 0 -> 1.52
  f32 fov;                   // 0 -> leave the Camera's
  f32 zoom_target;           // 0..1 ADS blend, written by the game
  f32 zoom_fov;              // degrees at full zoom; 0 -> fov
  f32 zoom_lambda;           // 0 -> 14
  f32 near_plane;
  f32 far_plane;
  f32 bob_height;            // 0 -> 0.045
  f32 bob_sway;              // 0 -> 0.025
  f32 bob_rate;              // bob phase per metre walked; 0 -> 1.35
  f32 bob_speed_ref;         // speed for full bob; 0 -> the target's run_speed
  f32 bob_zoom_damp;         // bob lost at full zoom; 0 -> 0.8
  f32 dip_stiffness;         // landing dip spring; 0 -> 160
  f32 dip_damping;           // 0 -> 14
  f32 dip_scale;             // dip impulse per m/s of fall speed; 0 -> 0.12
  f32 dip_max_speed;         // 0 -> 18
  f32 dip_min_speed;         // landings slower than this do not dip; 0 -> 3
  f32 punch_stiffness;       // 0 -> 220
  f32 punch_damping;         // 0 -> 18
  f32 punch_pitch;           // pitch per unit of punch; 0 -> 0.02
  f32 recoil_lambda;         // recoil offset decay; 0 -> 7
  f32 roll_strafe;           // roll per unit of strafe; 0 -> 0.012
  f32 roll_air_scale;        // roll while airborne; 0 -> 0.4
  f32 roll_lambda;           // 0 -> 8
  f32 run_fov_kick;          // degrees while running; 0 -> 6
  f32 fov_kick_lambda;       // 0 -> 5
  f32 slump_time;            // seconds to the full death slump; 0 -> 0.9
  f32 slump_drop;            // metres the eye falls; 0 -> 1.25
  f32 slump_pitch;           // 0 -> 0.35
  f32 slump_roll;            // 0 -> 0.6
  CameraShakeTuning shake_tuning;
  // runtime
  HZ_RUNTIME f32 zoom;
  HZ_RUNTIME f32 bob_phase;
  HZ_RUNTIME f32 move_amount;
  HZ_RUNTIME f32 roll;
  HZ_RUNTIME f32 fov_kick;
  HZ_RUNTIME f32 recoil_pitch;
  HZ_RUNTIME f32 recoil_yaw;
  HZ_RUNTIME f32 dip;
  HZ_RUNTIME f32 dip_velocity;
  HZ_RUNTIME f32 punch;
  HZ_RUNTIME f32 punch_velocity;
  HZ_RUNTIME f32 death_time;
  HZ_RUNTIME u32 landings_seen;
  HZ_RUNTIME CameraShake shake;
  HZ_RUNTIME b32 placed;
};

// the game writes this whole, every frame, before the rigs run
typedef struct CameraRigInput {
  v2 look;            // radians turned right / tilted up
  f32 zoom;           // log factor on the boom (Follow, Orbit)
  f32 shake;          // trauma added this frame
  v2 recoil;          // FirstPerson: spring offsets that decay, radians right / up
  f32 punch;          // FirstPerson: impulse into the punch spring
  f32 fov_kick;       // FirstPerson: degrees added to the FOV kick
  b32 dead;           // FirstPerson: death slump
  b32 cancel_fly;     // Orbit: cancels a fly-to
  b32 has_target;     // Follow without a target entity
  v3 target_position;
  v3 target_velocity;
  b32 has_target_rotation; // heading for the Follow yaw modes; without it yaw is free
  quat target_rotation;
} CameraRigInput;
ECS_COMPONENT_TYPEDEF(CameraRigInput);

// what the last rig that ran left on the camera
typedef struct CameraRigState {
  v3 position;
  quat rotation;
  v3 focus;
  f32 yaw;           // the view yaw in the rig's convention (forward = (-sin, 0, -cos) for Follow and Orbit-free)
  f32 pitch;
  f32 fov;
  f32 distance;
  f32 zoom;          // FirstPerson ADS blend
  f32 bob_phase;
  f32 move_amount;
  f32 shake;         // trauma^exponent
  b32 ready;
} CameraRigState;
ECS_COMPONENT_TYPEDEF(CameraRigState);

// places the orbit at `position` looking at `target`; a rig with no view (distance 0) leaves the Camera alone
void camera_orbit_set_view(CameraOrbit *co, v3 position, v3 target);
// main lane, once: the CameraRigInput and CameraRigState resources
void camera_rig_init(EcsWorld *w);
// inside the scope, after the systems that write CameraRigInput and the characters, before ecs_camera_schedule
void camera_rig_schedule(EcsWorld *w);
